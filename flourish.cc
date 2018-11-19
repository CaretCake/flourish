#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <regex>
#include <signal.h>

#include <readline/readline.h>
#include <readline/history.h>

using namespace std;

void startUp();
void ctrlC(int s);
void parseInput(string inputString, vector<string> &parsedInput);
bool bangCommandRegex(string bangCommand, vector<string> &parsedInput, char* &buf);
void replaceInput(vector<string> &parsedInput);
void changeDirectory(vector<string> &parsedInput);
void setEnvironmentVariable(vector<string> &parsedInput, char* &buf);
void fileIORedirection(vector<string> &parsedInput, int index, char direction);
vector<string> splitString(char delimiter, string target);
bool isBangCommand(string command);
bool isExit(string command);
bool isChangeDirectory(string command);
bool isEnvVarAssignment(string command);
bool isInputRedirection(string command);
bool isOutputRedirection(string command);
void printMessage(string msg, int type);
void execIt(char** parsedInput);

const char *PROMPT = "PROMPT";
const char *DEFAULT_PROMPT =  ">> ";
const string WELCOME_MESSAGE = "**************************************\n*********Welcome to FlouriSH!*********\n*********Type 'EXIT' to quit.*********\n**************************************\n";

int main(int argc, char *argv[]) {
    char* buf;
    vector<string> parsedInput;
    
    startUp();
    
    while ((buf = readline(getenv("PROMPT"))) != nullptr) { // readline library
        if (strlen(buf) > 0) {
            
            //split on ';' characters for executing queued commands, then execute each
            vector<string> splitCommands = splitString(';', buf);
            
            for (string command : splitCommands) {
                parsedInput.clear();
                strcpy(buf, command.c_str());
                bool commandOK = true;
                
                if (isBangCommand(buf)) {
                    commandOK = bangCommandRegex(splitString('!', buf)[1], parsedInput, buf);
                }
                
                if (commandOK) {
                    add_history(buf);
                    
                    parseInput(buf, parsedInput);
                    
                    if (isExit(parsedInput[0])) {
                        exit(0);
                    }
                    
                    // replace environment variables and tilde
                    replaceInput(parsedInput);
                    
                    if (isChangeDirectory(parsedInput[0])) {
                        changeDirectory(parsedInput);
                    }
                    else if (isEnvVarAssignment(parsedInput[0])) {
                        setEnvironmentVariable(parsedInput, buf);
                    }
                    // continue with normal exec
                    else {
                        switch (int id = fork()) {
                            case -1: { // failed fork
                                cout << "fork problems" << endl;
                                break;
                            }
                            case 0: { // child
                                // check size for possibility of file IO redirection
                                if (parsedInput.size() >= 3) {
                                    for (int i = 1; i < parsedInput.size(); i++) {
                                        if (isOutputRedirection(parsedInput[i])) {
                                            fileIORedirection(parsedInput, i, '>');
                                        }
                                        else if (isInputRedirection(parsedInput[i])) {
                                            fileIORedirection(parsedInput, i, '<');
                                        }
                                    }
                                }
                                
                                // transfer everything to vector of char*
                                vector<char*> cParsedInput;
                                cParsedInput.reserve(parsedInput.size());
                                for(size_t i = 0; i < parsedInput.size(); ++i)
                                    cParsedInput.push_back(const_cast<char*>(parsedInput[i].c_str()));
                                cParsedInput.push_back(NULL);
                                
                                //exec
                                execIt(&cParsedInput[0]);
                                
                                // exec failed if we get here
                                printMessage(parsedInput[0], 0);
                                exit(0);
                                break;
                            }
                            default: { // parent
                                waitpid(-1, NULL, 0);
                                break;
                            }
                        }
                    }
                }
            }
            free(buf);
        }
    }
}

// Run on start up to display welcome message, set up catching CTRL+C,
// and to set prompt environment variable to default.
void startUp() {
    cout << WELCOME_MESSAGE << endl;
    signal(SIGINT, ctrlC);
    if (!getenv(PROMPT)) {
        setenv(PROMPT, DEFAULT_PROMPT, 1);
    }
}

void ctrlC(int s) {
    //cout << "ye" << endl;
}

// Parses input string (buf) from user and divides each chunk of the command
// into parsedInput vector.
void parseInput(string inputString, vector<string> &parsedInput) {
    istringstream istream (inputString);
    string inputChunk;
    while (istream >> inputChunk) {
        parsedInput.push_back(inputChunk);
    }
}

// Returns a boolean indicating if the !bangcommand was a valid command in readline history
bool bangCommandRegex(string bangCommand, vector<string> &parsedInput, char* &buf) {
    // get history
    HISTORY_STATE *historyState = history_get_history_state ();
    HIST_ENTRY **historyList = history_list();
    
    // split ! from string
    bangCommand = splitString('!', buf)[1];
    
    if (regex_match(bangCommand, regex("([0-9]*)"))) { // if ![#]
        int commandsToGoBack = stoi(bangCommand);
        int historyListPos = historyState->length;
        if (commandsToGoBack > historyListPos) {
            printMessage(buf, 2);
            return false;
        }
        else {
            buf = historyList[historyListPos - commandsToGoBack]->line;
            cout << buf << endl;
            return true;
        }
    }
    else { // it's a ![character]
        for (int i = historyState->length - 1; i > 0; i--) {
            string line = historyList[i]->line;
            if (bangCommand == line.substr(0, bangCommand.length())) {
                buf = historyList[i]->line;
                cout << buf << endl;
                return true;
            }
            if (i == 0) {
                printMessage(buf, 2);
                return false;
            }
        }
    }
}

// Checks each entry in parsedInput for an environment variable or tilde and replaces
// them as needed.
void replaceInput(vector<string> &parsedInput) {
    for (int i = 0; i < parsedInput.size(); i++) {
        if (parsedInput[i][0] == '$') {
            parsedInput[i] = parsedInput[i].substr(1, parsedInput[i].length() - 1);
            if (getenv(parsedInput[i].c_str()) != NULL) {
                parsedInput[i] = getenv(parsedInput[i].c_str());
            }
            else {
                // if the environment variable does not exist, we just remove it from the command
                parsedInput.erase(parsedInput.begin() + i);
            }
        }
        int tildeReplaceLocation = parsedInput[i].find("~");
        if (tildeReplaceLocation != string::npos) {
            parsedInput[i].erase(tildeReplaceLocation, 1);
            parsedInput[i].insert(tildeReplaceLocation, getenv("HOME"));
        }
    }
}

void changeDirectory(vector<string> &parsedInput) {
    if  (chdir(parsedInput[1].c_str()) == -1) {
        printMessage(parsedInput[0], 1);
    }
}

void setEnvironmentVariable(vector<string> &parsedInput, char* &buf) {
    vector<string> splitAssignment = splitString('=', buf);
    
    setenv(splitAssignment[0].c_str(), splitAssignment[1].c_str(), 1);
}

void fileIORedirection(vector<string> &parsedInput, int index, char direction) {
    switch (direction) {
        case '>': {
            int fd = open(parsedInput[index+1].c_str(), O_RDWR | O_CREAT, 0660);
            if (fd == -1) {
                printMessage("open failed", 3);
                exit(0);
            }
            parsedInput.erase(parsedInput.begin() + index, parsedInput.end());
            dup2(fd, 1);
            break;
        }
        case '<': {
            int fd = open(parsedInput[index+1].c_str(), O_RDONLY);
            if (fd == -1) {
                printMessage("open failed", 3);
                exit(0);
            }
            parsedInput.erase(parsedInput.begin() + index, parsedInput.end());
            dup2(fd, 0);
            break;
        }
    }
}

// Returns a vector of strings that are the contents of the target string
// split on the given delimiter.
vector<string> splitString(char delimiter, string target) {
    vector<string> splitStrings;
    istringstream istream(target);
    string token;
    while (getline(istream, token, delimiter)) {
        splitStrings.push_back(token);
    }
    return splitStrings;
}

bool isBangCommand(string command) {
    return regex_match(command, regex("(!)(.+)"));
}

bool isExit(string command) {
    return (regex_match(command, regex("([[:space:]]*)(EXIT)([[:space:]]*)")) || command == "EXIT");
}

bool isChangeDirectory(string command) {
    return (command == "cd");
}

bool isEnvVarAssignment(string command) {
    return (regex_match(command, regex("(.+)(=)(.+)")));
}

bool isInputRedirection(string command) {
    return (command  == "<");
}

bool isOutputRedirection(string command) {
    return (command  == ">");
}

void printMessage(string msg, int type) {
    switch(type) {
        case 0: { // bad command
            cout << "flourish: " << msg << ": command not found" << endl;
            break;
        }
        case 1: { // bad chdir
            cout << "flourish: " << msg << ": no such directory" << endl;
            break;
        }
        case 2: { // bad !event
            cout << "flourish: " << msg << ": event not found" << endl;
            break;
        }
        case 3: { // bad file
            cout << "flourish: " << msg << ": file not found" << endl;
            break;
        }
    }
}

void execIt(char** parsedInput) {
    // attempt to execv on input for explicit file path
    execv(parsedInput[0], parsedInput);
    
    // attempt to execv on all possible paths in PATH environment variable
    vector<string> paths = splitString(':', getenv("PATH"));
    for (const auto& path : paths) {
        execv((path + "/" + parsedInput[0]).c_str(), parsedInput);
    }
}
