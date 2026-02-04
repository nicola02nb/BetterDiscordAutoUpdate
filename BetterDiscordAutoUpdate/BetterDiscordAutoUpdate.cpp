#include <iostream>
#include <filesystem>
#include <fstream>
#include <shlobj.h>
#include <sstream>
#include <windows.h>

using namespace std;
namespace fs = filesystem;

class DualStreamBuf : public streambuf {
public:
	DualStreamBuf(streambuf* sb1, streambuf* sb2) : sb1(sb1), sb2(sb2) {}

protected:
	virtual int overflow(int c) override {
		if (c == EOF) {
			return !EOF;
		}
		else {
			int const r1 = sb1->sputc(c);
			int const r2 = sb2->sputc(c);
			return r1 == EOF || r2 == EOF ? EOF : c;
		}
	}

	virtual int sync() override {
		int const r1 = sb1->pubsync();
		int const r2 = sb2->pubsync();
		return r1 == 0 && r2 == 0 ? 0 : -1;
	}

private:
	streambuf* sb1;
	streambuf* sb2;
};

enum DiscordChannel
{
	error = -1,
	stable,
	ptb ,
	canary
};

static void setupLogging(const string& logFilePath) {
	static ofstream logFile(logFilePath);
	static DualStreamBuf dualStreamBuf(cout.rdbuf(), logFile.rdbuf());
	cout.rdbuf(&dualStreamBuf);
}

static string getExecutablePath() {
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	return string(path);
}

static string getExecutableDir() {
	string exePath = getExecutablePath();
	return exePath.substr(0, exePath.find_last_of("\\/"));
}

static string getRoamingAppDataPath() {
	char path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
		return string(path);
	}
	return "";
}

static string getLocalAppDataPath() {
	char path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
		return string(path);
	}
	return "";
}

static void runProcess(const string& exePath, const string& args) {
	ShellExecuteA(NULL, "open", exePath.c_str(), args.empty() ? NULL : args.c_str(), NULL, SW_HIDE);
}

static void stopDiscord(const string& exeName) {
	cout << "Stopping " + exeName +"..." << endl;
	string command = "taskkill /f /im " + exeName + " >nul 2>&1";
	system(command.c_str());
}

static string getLatestDiscordAppPath(const string& discordPath) {
	vector<fs::path> appDirectories;
	for (const auto& entry : fs::directory_iterator(discordPath)) {
		if (entry.is_directory() && entry.path().filename().string().find("app-") == 0) {
			appDirectories.push_back(entry.path());
		}
	}

	if (appDirectories.empty()) {
		throw runtime_error("[ERROR] No app-* directories found in " + discordPath);
	}

	sort(appDirectories.begin(), appDirectories.end(), [](const fs::path& a, const fs::path& b) {
		return a.filename().string() > b.filename().string();
		});

	return appDirectories.front().string();
}

static string getLatestDiscordCorePath(const string& discordAppPath) {
	vector<fs::path> coreDirectories;
	for (const auto& entry : fs::directory_iterator(discordAppPath)) {
		if (entry.is_directory() && entry.path().filename().string().find("discord_desktop_core-") == 0) {
			coreDirectories.push_back(entry.path());
		}
	}
	if (coreDirectories.empty()) {
		throw runtime_error("[ERROR] No discord_desktop_core-* directories found in " + discordAppPath);
	}
	sort(coreDirectories.begin(), coreDirectories.end(), [](const fs::path& a, const fs::path& b) {
		return a.filename().string() > b.filename().string();
		});
	return coreDirectories.front().string() + "\\discord_desktop_core";
}

static string getLatestDiscordAppVersion(const string& discordPath) {
	string latestAppPath = getLatestDiscordAppPath(discordPath);
	return latestAppPath.substr(latestAppPath.find("app-") + 4);
}

static string getBetterDiscordDirectory() {
	return getRoamingAppDataPath() + "\\BetterDiscord";
}

static string escapeBackslashes(const string& input) {
	string output;
	for (char c : input) {
		if (c == '\\') {
			output += "\\\\";
		}
		else {
			output += c;
		}
	}
	return output;
}

static bool patchDiscord(const string& discordAppDirectory, const string& asarPath) {
	cout << "Patching Discord..." << endl;

	string indexPath = getLatestDiscordCorePath(discordAppDirectory + "\\modules") + "\\index.js";

	string escapedAsarPath = escapeBackslashes(asarPath);
	string toWrite = "require('" + escapedAsarPath + "');\nmodule.exports = require('./core.asar'); ";

	ofstream indexFile(indexPath, ios::trunc);
	if (indexFile.is_open()) {
		indexFile << toWrite;
		indexFile.close();

		cout << "Discord index.js Patched!" << endl;
		return true;
	}
	else {
		cout << "[ERROR] Unable to open file: " << indexPath << endl;
		return false;
	}
}

static string getBetterDiscordLatestVersion() {
	string url = "https://api.github.com/repos/BetterDiscord/BetterDiscord/releases/latest";
	string command = "curl -s " + url;
	string result;

	// Execute the curl command and capture the output
	FILE* pipe = _popen(command.c_str(), "r");
	if (!pipe) {
		throw runtime_error("[ERROR] Failed to execute curl command.");
	}

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		result += buffer;
	}
	_pclose(pipe);

	// Parse the JSON response to extract the "tag_name" field
	size_t tagPos = result.find("\"tag_name\": ");
	if (tagPos != string::npos) {
		size_t start = result.find("\"", tagPos + 12) + 2;
		size_t end = result.find("\"", start);
		if (start != string::npos && end != string::npos) {
			return result.substr(start, end - start);
		}
	}

	throw runtime_error("[ERROR] Failed to retrieve the latest BetterDiscord version.");
}

static void setBetterdiscordLocalVersion(const string& betterDiscordDirectory) {
  string version = getBetterDiscordLatestVersion();  
  string versionFilePath = betterDiscordDirectory + "\\version.txt";  

  ofstream versionFile(versionFilePath, ios::trunc);  
  if (versionFile.is_open()) {  
      versionFile << version;  
      versionFile.close();  
      cout << "BetterDiscord local version set to: " << version << endl;  
  } else {  
      cout << "[ERROR] Unable to write to version file: " << versionFilePath << endl;  
  }  
}

static bool downloadBetterDiscord(const string& asarDestinationPath) {
	cout << "Downloading betterdiscord.asar to " + asarDestinationPath + "..." << endl;
	string url = "https://github.com/BetterDiscord/BetterDiscord/releases/latest/download/betterdiscord.asar";
	string command = "curl -L -o \"" + asarDestinationPath + "\" \"" + url + "\"";
	system(command.c_str());
	return fs::exists(asarDestinationPath);
}

static string getBetterDiscordLocalVersion(const string& betterDiscordDirectory) {
   string versionFilePath = betterDiscordDirectory + "\\version.txt";  

   if (!fs::exists(versionFilePath)) {
       return "";  
   }  

   ifstream versionFile(versionFilePath);  
   if (versionFile.is_open()) {  
       string version;  
       getline(versionFile, version);  
       versionFile.close();  
       return version;  
   }  

   throw runtime_error("[ERROR] Unable to read version.txt.");  
}

static bool isAlreadyPatched(const string& discordAppDirectory) {
    string indexPath = getLatestDiscordCorePath(discordAppDirectory + "\\modules") + "\\index.js";
    ifstream indexFile(indexPath);
    if (indexFile.is_open()) {
        stringstream buffer;
        buffer << indexFile.rdbuf();
        indexFile.close();
        return buffer.str().find("betterdiscord.asar") != string::npos;
    }
    return false;
}

static bool isBetterDiscordUpToDate(const string& betterDiscordDirectory) {
	string latestVersion = getBetterDiscordLatestVersion();
	string versionFilePath = getBetterDiscordLocalVersion(betterDiscordDirectory);
	cout << "Latest Version: " << latestVersion << endl;
	cout << "Local Version: " << versionFilePath << endl;
	return latestVersion == versionFilePath;
}

static bool installBetterDiscord(const string& betterDiscordDirectory, const string& discordAppDirectory) {
	cout << "Installing BetterDiscord..." << endl;

	if (!fs::exists(betterDiscordDirectory)) {
		if (!fs::create_directory(betterDiscordDirectory)){
			cout << "[ERROR] Failed to create BetterDiscord directory!" << endl;
			return false;
		}
	}
	string data = betterDiscordDirectory + "\\data";
	if (!fs::exists(data)) {
		if (!fs::create_directory(data)){
			cout << "[ERROR] Failed to create BetterDiscord data directory!" << endl;
			return false;
		}
	}
	string asar = data + "\\betterdiscord.asar";
	if (!fs::exists(asar)) {
		cout << "Downloading BetterDiscorde..." << endl;
		if (downloadBetterDiscord(asar)) {
			setBetterdiscordLocalVersion(betterDiscordDirectory);
			cout << "BetterDiscord downloaded!" << endl;
		}
		else {
			cout << "[ERROR] Failed to download BetterDiscord!" << endl;
			return false;
		}
	}
	else {
		if (!isBetterDiscordUpToDate(betterDiscordDirectory)) {
			cout << "Updating BetterDiscord..." << endl;
			if (downloadBetterDiscord(asar)) {
				setBetterdiscordLocalVersion(betterDiscordDirectory);
				cout << "BetterDiscord updated!" << endl;
			}
			else {
				cout << "[ERROR] Failed to update BetterDiscord!" << endl;
				return false;
			}
		}
		else
		{
			cout << "File betterdiscord.asar was already downloaded and up-to-date!" << endl;
		}
	}

	cout << "Using betterdiscord.asar at: " << asar << endl;
	if (isAlreadyPatched(discordAppDirectory)) {
		cout << "BetterDiscord Was Already Installed!" << endl;
	}
	else if (patchDiscord(discordAppDirectory, asar)) {
		cout << "BetterDiscord Installed!" << endl;
	}
	else {
		cout << "[ERROR] Failed to patch Discord!" << endl;
		return false;
	}
	return true;
}

static string toLower(const string& str) {
	string lowerStr = str;
	transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
	return lowerStr;
}

static string getChannelStringFromType(const DiscordChannel& channel) {
	if (channel == DiscordChannel::stable) {
		return "stable";
	}
	else if (channel == DiscordChannel::ptb) {
		return "ptb";
	}
	else if (channel == DiscordChannel::canary) {
		return "canary";
	}
	else {
		return "ErrorDiscordType";
	}
}

static string getDiscordStringFromType(const DiscordChannel& channel) {
	if (channel == DiscordChannel::stable) {
		return "Discord";
	}
	else if (channel == DiscordChannel::ptb) {
		return "DiscordPTB";
	}
	else if (channel == DiscordChannel::canary) {
		return "DiscordCanary";
	}
	else {
		return "ErrorDiscordType";
	}
}

static string getDiscordDirectory(const DiscordChannel& channel) {
	return getLocalAppDataPath() + "\\" + getDiscordStringFromType(channel);
}

static string getDiscordExeName(const DiscordChannel channel) {
	return getDiscordStringFromType(channel) + ".exe";
}

static void proxyExecutable(const string& autoUpdatePath, const string& updatePath, const string& updateMovedPath, const boolean& overwrite = true) {
	cout << "Proxying " + updatePath << endl;
	cout <<"\tas " + updateMovedPath << endl;
	cout << "\twith " + autoUpdatePath << endl;

	if (fs::exists(updatePath) || fs::exists(updateMovedPath)) {
		if (!fs::exists(updateMovedPath)) {
			cout << "Renaming " + updatePath + " to " + updateMovedPath + "..." << endl;
			fs::rename(updatePath, updateMovedPath);
		}
		else {
			cout << updateMovedPath + " already exists..." << endl;
		}
		cout << "Moving " + autoUpdatePath + " to " + updatePath + "..." << endl;
		try {
			fs::copy_file(autoUpdatePath, updatePath, overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none);
		}
		catch (const fs::filesystem_error& e) {
			cout << "[INFO] Overwrite of " + updatePath + "Prevented" << endl;
		}
	}
	else {
		cout << "[ERROR] " + updatePath + " not found" <<  endl;
	}
}

static DiscordChannel getChannel(int argc, char* argv[]) {
	for (int i = 1; i < argc; ++i) {
		string arg = toLower(argv[i]);
		if (arg.find("--stable") != string::npos) {
			return DiscordChannel::stable;
		} else if (arg.find("--ptb") != string::npos) {
			return DiscordChannel::ptb;
		}
		else if (arg.find("--canary") != string::npos) {
			return DiscordChannel::canary;
		}
	}
	return DiscordChannel::error;
}

static bool shouldSkipInstallation(int argc, char* argv[]) {
	for (int i = 1; i < argc; ++i) {
		string arg = toLower(argv[i]);
		if (arg.find("--auto-update-skip-installation") != string::npos) {
			return true;
		}
	}
	return false;
}

static string getArgs(int argc, char* argv[]) {
	string args;
	for (int i = 1; i < argc; ++i) {
		args += argv[i];
		args += " ";
	}
	return args;
}

int main(int argc, char* argv[]) {
    string executed = argv[0];
	string args = getArgs(argc, argv);
	DiscordChannel channel = DiscordChannel::error;

	string autoUpdateDirectory = getExecutableDir();
	string autoUpdatePath = getExecutablePath();

	bool isProxied = string(argv[0]).find("BetterDiscordAutoUpdate") == string::npos;
	bool isLaunchingDiscord = string(argv[0]).find("Update.exe") != string::npos;
	bool manualRun = argc == 1;
	bool skipInstallation = shouldSkipInstallation(argc, argv);
	DiscordChannel cliChannel = getChannel(argc, argv);

	setupLogging(autoUpdateDirectory + "\\BetterDiscordAutoUpdate.log");

    cout << "BetterDiscord Auto Update" << endl << endl;

	cout << "AutoUpdate Directory: " << autoUpdateDirectory << endl;
	cout << "AutoUpdate Path: " << autoUpdatePath << endl << endl;

	cout << "Executed: " << executed << endl;
	cout << "Arguments: " << args << endl;
	cout << "Is Proxied: " << (isProxied ? "True" : "False") << endl;
	cout << "Is Launching Discord: " << (isLaunchingDiscord ? "True" : "False") << endl;
	cout << "Skip Installation: " << (skipInstallation ? "True" : "False") << endl;
	cout << "Manual Run: " << (manualRun ? "True" : "False") << endl << endl;

	if (!isProxied) {
		if (cliChannel != DiscordChannel::error) {
			channel = cliChannel;
			cout << "Cli Channel: " << getChannelStringFromType(cliChannel) << " (" << to_string(cliChannel) << ")" << endl;
		} else if (manualRun) {
			string input;
			cout << "Enter the Discord Channel to install (stable, ptb, canary): ";
			cin >> input;
			if (input == "stable") {
				channel = DiscordChannel::stable;
			}
			else if (input == "ptb") {
				channel = DiscordChannel::ptb;
			}
			else if (input == "canary") {
				channel = DiscordChannel::canary;
			}
			else {
				cout << "[ERROR] Invalid channel argument provided! Use stable, ptb or canary." << endl;
				return 1;
			}
			cout << "Manual Channel: " << getChannelStringFromType(channel) << " (" << to_string(channel) << ")" << endl << endl;
		}
		else {
			cout << "[ERROR] No channel argument provided! Use --stable, --ptb or --canary." << endl;
			return 1;
		}
		
		string discordDirectory = getDiscordDirectory(channel);
		string discordExeName = getDiscordExeName(channel);
		string discordUpdateDir = discordDirectory + "\\Update.exe";
		string discordUpdateMovedDir = discordDirectory + "\\Update.moved.exe";

		cout << "Installing Channel:\t\t\t" << getChannelStringFromType(channel) << " (" << to_string(channel) << ")" << endl;
		cout << "BetterDiscordAutoUpdate.exe Directory:\t" << autoUpdateDirectory << endl;
		cout << "BetterDiscordAutoUpdate.exe Path:\t" << autoUpdatePath << endl;
		cout << "Discord Directory:\t\t\t" << discordDirectory << endl;
		cout << "Discord Executable Name:\t\t" << discordExeName << endl;
		cout << "Discord Update.exe Path:\t" << discordUpdateDir << endl;
		cout << "Discord Update.moved.exe Path:\t" << discordUpdateMovedDir << endl;
		cout << endl;

		if (autoUpdateDirectory != discordDirectory) {
			proxyExecutable(autoUpdatePath, discordUpdateDir, discordUpdateMovedDir);
			cout << endl;
		}
		/*if (autoUpdateDirectory != discordAppDirectory) { //Was an idea to fix BD not installing at first Discord execution, but I don't like it....
			stopDiscord(discordExeName);
			stopDiscord(toExecute);
			proxyExecutable(discordAppDirectory, discordExeName, autoUpdatePath, false);
		}*/

		stopDiscord(discordExeName);
		cout << "Launching Discord Update.exe to continue the process..." << endl << endl;
		runProcess(discordDirectory + "\\Update.exe", "--processStart " + discordExeName);

		if (manualRun) {
			cout << "You can find this log at: " << autoUpdateDirectory + "\\BetterDiscordAutoUpdate.log" << endl << endl;
			cout << "Press any key to exit..." << endl;
			cin.get();
			cin.get();
		}
	}
	else if (isProxied && isLaunchingDiscord) {
		string discordUpdateMovedDir = executed.insert(executed.find(".exe"), ".moved");
		string betterDiscordDirectory = getBetterDiscordDirectory();
		cout << "Discord Update.moved.exe Path: " << discordUpdateMovedDir << endl;
		cout << "BetterDiscord Directory:\t\t" << betterDiscordDirectory << endl;

		if (skipInstallation) {
			cout << "Skipping installation as --auto-update-skip-installation argument was provided." << endl;
			cout << "Launching :" << discordUpdateMovedDir << " to continue the process..." << endl << endl;
			runProcess(discordUpdateMovedDir, args);
		}
		else {
			string discordDirectory = autoUpdateDirectory;
			string discordAppDirectory = getLatestDiscordAppPath(discordDirectory);

			cout << "Discord Directory:\t\t\t" << discordDirectory << endl;
			cout << "Discord App Directory:\t\t" << discordAppDirectory << endl;

			do {
				if (installBetterDiscord(betterDiscordDirectory, discordAppDirectory)) {
					//stopDiscord("Update.exe");
				}
				cout << "Launching :" << discordUpdateMovedDir << " to continue the process..." << endl << endl;
				runProcess(discordUpdateMovedDir, args);
			} while (!isAlreadyPatched(autoUpdateDirectory));
		}
	}
	else {
		cout << "[ERROR] Invalid execution context detected! This executable is meant to be launched by Discord's Update.exe as a proxy for the Discord executable. If you want to run this manually, simply execute BetterDiscordAutoUpdate.exe and follow the instructions." << endl;
	}

    return 0;
}
