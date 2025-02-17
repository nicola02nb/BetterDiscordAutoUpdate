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
	cout << "Stopping Discord..." << endl;
	string command = "taskkill /f /im " + exeName;
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
		throw runtime_error("No app-* directories found in " + discordPath);
	}

	sort(appDirectories.begin(), appDirectories.end(), [](const fs::path& a, const fs::path& b) {
		return a.filename().string() > b.filename().string();
		});

	return appDirectories.front().string();
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

	string indexPath = discordAppDirectory + "\\modules\\discord_desktop_core-1\\discord_desktop_core\\index.js";

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
		cout << "Unable to open file: " << indexPath << endl;
		return false;
	}
}

void downloadBetterDiscord(const string& asarDestinationPath) {
	cout << "Downloading betterdiscord.asar into Betterdiscord\\data Folder..." << endl;
	string url = "https://github.com/BetterDiscord/BetterDiscord/releases/latest/download/betterdiscord.asar";
	string command = "curl -L -o \"" + asarDestinationPath + "\" \"" + url + "\"";
	system(command.c_str());
}

bool isAlreadyPatched(const string& betterDiscordDirectory, const string& discordExeName, const string& appVersion) {
	string lastPatchedVersionFile = betterDiscordDirectory + "\\lastPatchedVersion.txt";
	string name = discordExeName.substr(0, discordExeName.find(".exe"));
	string toWrite = name + " " + appVersion;
	if (!fs::exists(lastPatchedVersionFile)) {
		ofstream fileCreate(lastPatchedVersionFile);
		fileCreate.close();
	}

	// Check if the file exists
	if (fs::exists(lastPatchedVersionFile)) {
		// Read the current content of the file
		ifstream fileRead(lastPatchedVersionFile);
		stringstream buffer;
		buffer << fileRead.rdbuf();
		string currentContent = buffer.str();
		fileRead.close();

		// Check if the file contains the string toWrite
		if (currentContent.find(toWrite) != string::npos) {
			return true;
		}
	}

	// Write to the file if it doesn't contain the string
	ofstream fileWrite(lastPatchedVersionFile, ios::app);
	if (fileWrite.is_open()) {
		fileWrite << toWrite << endl;
		fileWrite.close();
	}
	else {
		cout << "Unable to open file: " << lastPatchedVersionFile << endl;
	}
	return false;
}

void installBetterDiscord(const string& betterDiscordDirectory, const string& discordDirectory, const string& discordExeName) {
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
		cout << "betterdiscord.asar Already Downloaded!" << endl;
	}

	string latestAppPath = getLatestDiscordAppPath(discordDirectory);
	string appVersion = latestAppPath.substr(latestAppPath.find("app-") + 4);
	if (isAlreadyPatched(betterDiscordDirectory, discordExeName, appVersion)) {
		cout << "BetterDiscord Was Already Installed!" << endl;
	}
	else if (patchDiscord(latestAppPath, asar)) {
		stopDiscord(discordExeName);
		cout << "BetterDiscord Installed!" << endl;
	}
	else {
		cout << "Failed to patch Discord!" << endl;
	}
	cout << endl;
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

int main(int argc, char* argv[]) {
	string channel = getChannel(argc, argv);
	string autoUpdateDirectory = getExecutableDir();
	string autoUpdatePath = getExecutablePath();
	string discordDirectory = getDiscordFolder(channel);
	string discordExeName = getDiscordExeName(channel);
	string betterDiscordDirectory = getBetterDiscordFolder();

	setupLogging(betterDiscordDirectory + "\\BetterDiscordAutoUpdate.log");


	cout << "BetterDiscord Auto Update" << endl << endl;

	cout << "Installing Channel:\t\t\t" << channel << endl;
	cout << "BetterDiscordAutoUpdate.exe Directory:\t" << autoUpdateDirectory << endl;
	cout << "BetterDiscordAutoUpdate.exe Path:\t" << autoUpdatePath << endl;
	cout << "Discord Directory:\t\t\t" << discordDirectory << endl;
	cout << "BetterDiscord Directory:\t\t" << betterDiscordDirectory << endl;
	cout << endl;

	string updateMoved = discordDirectory + "\\Update.moved.exe";
	if (autoUpdateDirectory != discordDirectory) {
		string updateExe = discordDirectory + "\\Update.exe";
		string newUpdateExe = discordDirectory + "\\Update.exe";

		if (fs::exists(updateExe)) {
			if (!fs::exists(updateMoved)) {
				cout << "Renaming Update.exe from Discord Folder to Update.moved.exe..." << endl;
				fs::rename(updateExe, updateMoved);
			}
		}
		
		cout << "Moving BetterDiscordAutoUpdate.exe to Discord Folder as Update.exe..." << endl << endl;
		fs::copy_file(autoUpdatePath, newUpdateExe, fs::copy_options::overwrite_existing);
	}

	do {
		installBetterDiscord(betterDiscordDirectory, discordDirectory, discordExeName);
		runProcess(updateMoved, "--processStart " + discordExeName);
	} while (!isAlreadyPatched(betterDiscordDirectory, discordExeName, getLatestDiscordAppVersion(discordDirectory)));

	if (argc == 1) {
		cout << "You can fint this log at: " << betterDiscordDirectory + "\\BetterDiscordAutoUpdate.log" << endl << endl;
		cout << "Press any key to exit..." << endl;
		cin.get();
	}

	return 0;
}
