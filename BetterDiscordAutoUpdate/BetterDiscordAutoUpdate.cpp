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

void setupLogging(const string& logFilePath) {
	static ofstream logFile(logFilePath);
	static DualStreamBuf dualStreamBuf(cout.rdbuf(), logFile.rdbuf());
	cout.rdbuf(&dualStreamBuf);
}

string getExecutablePath() {
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	return string(path);
}

string getExecutableDir() {
	string exePath = getExecutablePath();
	return exePath.substr(0, exePath.find_last_of("\\/"));
}

string getRoamingAppDataPath() {
	char path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
		return string(path);
	}
	return "";
}

string getLocalAppDataPath() {
	char path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
		return string(path);
	}
	return "";
}

void runProcess(const string& exePath, const string& args) {
	ShellExecuteA(NULL, "open", exePath.c_str(), args.empty() ? NULL : args.c_str(), NULL, SW_HIDE);
}

void stopDiscord(const string& exeName) {
	cout << "Stopping " + exeName +"..." << endl;
	string command = "taskkill /f /im " + exeName + " >nul 2>&1";
	system(command.c_str());
}

string getLatestDiscordAppPath(const string& discordPath) {
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

string getLatestDiscordCorePath(const string& discordAppPath) {
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

string getLatestDiscordAppVersion(const string& discordPath) {
	string latestAppPath = getLatestDiscordAppPath(discordPath);
	return latestAppPath.substr(latestAppPath.find("app-") + 4);
}

string getBetterDiscordFolder() {
	return getRoamingAppDataPath() + "\\BetterDiscord";
}

string escapeBackslashes(const string& input) {
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

bool patchDiscord(const string& discordAppDirectory, const string& asarPath) {
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

void downloadBetterDiscord(const string& asarDestinationPath) {
	cout << "Downloading betterdiscord.asar into Betterdiscord\\data Folder..." << endl;
	string url = "https://github.com/BetterDiscord/BetterDiscord/releases/latest/download/betterdiscord.asar";
	string command = "curl -L -o \"" + asarDestinationPath + "\" \"" + url + "\"";
	system(command.c_str());
}

bool isAlreadyPatched(const string& discordAppDirectory) {
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

bool installBetterDiscord(const string& betterDiscordDirectory, const string& discordAppDirectory) {
	cout << "Installing BetterDiscord..." << endl;

	if (!fs::exists(betterDiscordDirectory)) {
		fs::create_directory(betterDiscordDirectory);
	}
	string data = betterDiscordDirectory + "\\data";
	if (!fs::exists(data)) {
		fs::create_directory(data);
	}
	string asar = data + "\\betterdiscord.asar";
	if (!fs::exists(asar)) {
		downloadBetterDiscord(asar);
	}
	else {
		cout << "File betterdiscord.asar Was Already Downloaded!" << endl;
	}

	if (isAlreadyPatched(discordAppDirectory)) {
		cout << "BetterDiscord Was Already Installed!" << endl;
	}
	else if (patchDiscord(discordAppDirectory, asar)) {
		cout << "BetterDiscord Installed!" << endl << endl;
		return true;
	}
	else {
		cout << "[ERROR] Failed to patch Discord!" << endl;
	}
	cout << endl;
	return false;
}

string toLower(const string& str) {
	string lowerStr = str;
	transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
	return lowerStr;
}

string getChannel(int argc, char* argv[]) {
	for (int i = 1; i < argc; ++i) {
		string arg = toLower(argv[i]);
		if (arg.find("canary") != string::npos) {
			return "canary";
		}
		else if (arg.find("ptb") != string::npos) {
			return "ptb";
		}
	}
	return "stable";
}

string getDiscordFolder(const string& channel) {
	if (channel == "canary") {
		return getLocalAppDataPath() + "\\DiscordCanary";
	}
	else if (channel == "ptb") {
		return getLocalAppDataPath() + "\\DiscordPTB";
	}
	else {
		return getLocalAppDataPath() + "\\Discord";
	}
}

string getDiscordExeName(const string& channel) {
	if (channel == "canary") {
		return "DiscordCanary.exe";
	}
	else if (channel == "ptb") {
		return "DiscordPTB.exe";
	}
	else {
		return "Discord.exe";
	}
}

string renameExe(const string& exeName) {
	string newName = exeName;
	newName.insert(newName.find(".exe"), ".moved");
	return newName;
}

void proxyExecutable(const string& discordDirectory, const string& exeName, const string& autoUpdatePath, bool overwrite = true) {
	cout << "Proxying " + exeName + "..." << endl;
	string exe = discordDirectory + "\\" + exeName;
	string renamedExe = renameExe(exeName);
	string exeMoved = discordDirectory + "\\" + renamedExe;

	if (fs::exists(exe) || fs::exists(exeMoved)) {
		if (!fs::exists(exeMoved)) {
			cout << "Renaming " + exeName + " from Discord Folder to " + renamedExe + "..." << endl;
			fs::rename(exe, exeMoved);
		}
		else {
			cout << renamedExe + " already exists in Discord Folder..." << endl;
		}
		cout << "Moving BetterDiscordAutoUpdate.exe to Discord Folder as " + exeName + "..." << endl;
		try {
			fs::copy_file(autoUpdatePath, exe, overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none);
		}
		catch (const fs::filesystem_error& e) {
			cout << "[INFO] Overwrite of " + exeName + "Prevented" << endl;
		}
	}
	else {
		cout << "[ERROR] " + exeName + " not found in at " + discordDirectory <<  endl;
	}
	cout << endl;
}

string getArgs(int argc, char* argv[]) {
	string args;
	for (int i = 1; i < argc; ++i) {
		args += argv[i];
		args += " ";
	}
	return args;
}

int main(int argc, char* argv[]) {
    string executed = argv[0];
	string toExecute = renameExe(executed);
	string args = getArgs(argc, argv);
    string channel;

	bool manuallyRun = argc == 1 && string(argv[0]).find("BetterDiscordAutoUpdate") != string::npos;

    channel = getChannel(argc, argv);

	if (manuallyRun) {
		string input;
		cout << "Enter the Discord Channel to install (canary, ptb, stable): ";
		cin >> input;
		if (input == "canary" || input == "ptb" || input == "stable") {
			channel = input;
		}
		else {
			cout << "Invalid Channel! Defaulting to stable..." << endl;
			return 1;
		}
	}

    string autoUpdateDirectory = getExecutableDir();
    string autoUpdatePath = getExecutablePath();

    string discordDirectory = getDiscordFolder(channel);
    string discordAppDirectory = getLatestDiscordAppPath(discordDirectory);
    string discordExeName = getDiscordExeName(channel);
    string discordUpdateExeName = "Update.exe";

    string betterDiscordDirectory = getBetterDiscordFolder();

    setupLogging(betterDiscordDirectory + "\\BetterDiscordAutoUpdate.log");

    cout << "BetterDiscord Auto Update" << endl << endl;

    cout << "Installing Channel:\t\t\t" << channel << endl;
    cout << "BetterDiscordAutoUpdate.exe Directory:\t" << autoUpdateDirectory << endl;
    cout << "BetterDiscordAutoUpdate.exe Path:\t" << autoUpdatePath << endl;
    cout << "Discord Directory:\t\t\t" << discordDirectory << endl;
	cout << "Discord App Directory:\t\t" << discordAppDirectory << endl;
    cout << "BetterDiscord Directory:\t\t" << betterDiscordDirectory << endl;
    cout << endl;

    if (autoUpdateDirectory != discordDirectory) {
        proxyExecutable(discordDirectory, discordUpdateExeName, autoUpdatePath);
    }
	/*if (autoUpdateDirectory != discordAppDirectory) { //Was an idea to fix BD not installing at first Discord execution, but I don't like it....
		stopDiscord(discordExeName);
		stopDiscord(toExecute);
        proxyExecutable(discordAppDirectory, discordExeName, autoUpdatePath, false);
    }*/

	do {
		if (installBetterDiscord(betterDiscordDirectory, discordAppDirectory)) {
			stopDiscord(discordExeName);
		}
		runProcess(toExecute, args);
	} while (!isAlreadyPatched(discordAppDirectory));

	if (manuallyRun) {
		runProcess(discordDirectory + "\\Update.exe", "--processStart "+discordExeName);
		cout << "You can find this log at: " << betterDiscordDirectory + "\\BetterDiscordAutoUpdate.log" << endl << endl;
		cout << "Press any key to exit..." << endl;
		cin.get();
	}

    return 0;
}
