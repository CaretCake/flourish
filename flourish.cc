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
bool bangCommandRegex(string bangCommand, vector<string> &parsedInput, char* &buf);
void parseInput(string inputString, vector<string> &parsedInput);
vector<string> splitString(char delimiter, string target);
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
            
            vector<string> splitCommands = splitString(';', buf);
            
            for (string command : splitCommands) {
                parsedInput.clear();
                strcpy(buf, command.c_str());
                bool commandOK = true;
                
                if (regex_match(buf, regex("(!)(.*)"))) { // if !bang at start of input, execute last command accordingly
                    
                    //commandOK = bangCommandRegex(splitString('!', buf)[1], parsedInput, buf);
                    
                    // get history
                    HISTORY_STATE *historyState = history_get_history_state ();
                    HIST_ENTRY **historyList = history_list();
                    
                    // split ! from string
                    string bangCommand = splitString('!', buf)[1];
                    
                    if (regex_match(bangCommand, regex("([0-9]*)"))) { // if ![#]
                        int commandsToGoBack = stoi(bangCommand);
                        int historyListPos = historyState->length;
                        if (commandsToGoBack > historyListPos) {
                            printMessage(buf, 2);
                            commandOK = false;
                        }
                        else {
                            buf = historyList[historyListPos - commandsToGoBack]->line;
                            cout << buf << endl;
                        }
                    }
                    else { // it's a ![character]
                        // for each in the list from back to begin
                        for (int i = historyState->length - 1; i > 0; i--) {
                            string line = historyList[i]->line;
                            if (bangCommand == line.substr(0, bangCommand.length())) {
                                //change it
                                buf = historyList[i]->line;
                                cout << buf << endl;
                                break;
                            }
                            if (i == 0) {
                                printMessage(buf, 2);
                                commandOK = false;
                            }
                        }
                    }
                }
                
                if (commandOK) {
                    add_history(buf);
                    // parse input for arguments
                    parseInput(buf, parsedInput);
                    
                    if (regex_match(parsedInput[0], regex("([[:space:]]*)(EXIT)([[:space:]]*)")) || parsedInput[0] == "EXIT") {
                        return(0);
                    }
                    
                    // check for environment variables and replace them if they exist, remove from command if non existent
                    for (int i = 0; i < parsedInput.size(); i++) { // loop through all input chunks in command
                        if (parsedInput[i][0] == '$') { // if the first character is a $VARIABLE
                            parsedInput[i] = parsedInput[i].substr(1, parsedInput[i].length() - 1); // remove the $
                            if (getenv(parsedInput[i].c_str()) != NULL) { // check if the environment variable exists
                                parsedInput[i] = getenv(parsedInput[i].c_str()); // if so, replace input chunk with variable value
                            }
                            else { // if not erase input chunk from the command vector
                                parsedInput.erase(parsedInput.begin() + i);
                            }
                        }
                        // check for occurences of ~ and replace any found with home directory
                        int tildeReplaceLocation = parsedInput[i].find("~");
                        if (tildeReplaceLocation != string::npos) {
                            parsedInput[i].erase(tildeReplaceLocation, 1);
                            parsedInput[i].insert(tildeReplaceLocation, getenv("HOME"));
                        }
                    }
                    
                    if (parsedInput[0] == "cd") { // if cd command, chdir
                        if  (chdir(parsedInput[1].c_str()) == -1) {
                            printMessage(parsedInput[0], 1);
                        }
                    }
                    else if (regex_match(parsedInput[0], regex("(.*)(=)(.*)"))) { // else if it's an = assignment
                        vector<string> splitAssignment = splitString('=', buf);
                        
                        // use setenv
                        setenv(splitAssignment[0].c_str(), splitAssignment[1].c_str(), 1);
                    }
                    else {
                        switch (int id = fork()) {
                            case -1: { // failed fork
                                cout << "fork problems" << endl;
                                break;
                            }
                            case 0: { // child
                                // dup must happen here
                                if (parsedInput.size() >= 3) {
                                    
                                    for (int i = 1; i < parsedInput.size(); i++) {
                                        //cout << inputChunk << endl;
                                        if (parsedInput[i] == ">") {
                                            int fd = open(parsedInput[i+1].c_str(), O_RDWR | O_CREAT, 0660);
                                            parsedInput.erase(parsedInput.begin() + i, parsedInput.end());
                                            //cout << parsedInput[0] << endl;
                                            // dup2
                                            dup2(fd, 1);
                                            // remove rest from the arg list
                                        }
                                        else if (parsedInput[i] == "<") {
                                            int fd = open(parsedInput[i+1].c_str(), O_RDONLY);
                                            parsedInput.erase(parsedInput.begin() + i, parsedInput.end());
                                            //cout << parsedInput[0] << endl;
                                            // dup2
                                            dup2(fd, 0);
                                            // remove rest from the arg list
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
                                // failed exec
                                printMessage(parsedInput[0], 0);
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

bool bangCommandRegex(string bangCommand, vector<string> &parsedInput, char* &buf) {
    // get history
    HISTORY_STATE *historyState = history_get_history_state ();
    HIST_ENTRY **historyList = history_list();
    
    if (regex_match(bangCommand, regex("([0-9]*)"))) { // if it's !#
        int commandsToGoBack = stoi(bangCommand);
        int historyListPos = historyState->length - 1;
        if (commandsToGoBack > historyListPos) { // outside of bounds of history
            printMessage(buf, 2);
            return false;
        }
        buf = historyList[historyListPos - commandsToGoBack]->line;
        return true;
    }
    else { // it's a ![character]
        // for each in the list from back to begin
        for (int i = historyState->length - 1; i > 0; i--) { // check backward in history
            string line = historyList[i]->line;
            if (bangCommand == line.substr(0, bangCommand.length())) {
                //change it
                buf = historyList[i]->line;
                return true;
            }
            if (i == 0) {
                printMessage(buf, 2);
                return false;
            }
        }
    }
}

void parseInput(string inputString, vector<string> &parsedInput) {
    istringstream istream (inputString);
    string inputChunk;
    while (istream >> inputChunk) {
        parsedInput.push_back(inputChunk);
    }
}

vector<string> splitString(char delimiter, string target) {
    vector<string> splitStrings;
    istringstream istream(target);
    string token;
    while (getline(istream, token, delimiter)) {
        splitStrings.push_back(token);
    }
    return splitStrings;
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
