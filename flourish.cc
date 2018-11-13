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
                
                if (regex_match(buf, regex("(!)(.*)"))) { // if !bang at start of input, execute last command accordingly
                    commandOK = bangCommandRegex(splitString('!', buf)[1], parsedInput, buf);
                }
                
                if (commandOK) {
                    add_history(buf);
                    // parse input for arguments
                    parseInput(buf, parsedInput);
                    
                    //check for EXIT command
                    if (regex_match(parsedInput[0], regex("([[:space:]]*)(EXIT)([[:space:]]*)")) || parsedInput[0] == "EXIT") {
                        return(0);
                    }
                    
                    // check for environment variables and replace them if they exist, remove from command if non existent
                    replaceInput(parsedInput);
                    
                    // check for a cd call
                    if (parsedInput[0] == "cd") {
                        changeDirectory(parsedInput);
                    }
                    // check for an environment variable assignment
                    else if (regex_match(parsedInput[0], regex("(.*)(=)(.*)"))) {
                        setEnvironmentVariable(parsedInput, buf);
                    }
                    // exec as normal
                    else {
                        switch (int id = fork()) {
                            case -1: { // failed fork
                                cout << "fork problems" << endl;
                                break;
                            }
                            case 0: { // child
                                if (parsedInput.size() >= 3) { // check size for possibility of file IO redirection
                                    // check for file redirection
                                    for (int i = 1; i < parsedInput.size(); i++) {
                                        if (parsedInput[i] == ">") {
                                            fileIORedirection(parsedInput, i, '>');
                                        }
                                        else if (parsedInput[i] == "<") {
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

void parseInput(string inputString, vector<string> &parsedInput) {
    istringstream istream (inputString);
    string inputChunk;
    while (istream >> inputChunk) {
        parsedInput.push_back(inputChunk);
    }
}

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
        // for each in the list from back to begin
        for (int i = historyState->length - 1; i > 0; i--) {
            string line = historyList[i]->line;
            if (bangCommand == line.substr(0, bangCommand.length())) {
                //change it
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

void replaceInput(vector<string> &parsedInput) {
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
            parsedInput.erase(parsedInput.begin() + index, parsedInput.end());
            dup2(fd, 1);
            break;
        }
        case '<': {
            int fd = open(parsedInput[index+1].c_str(), O_RDONLY);
            parsedInput.erase(parsedInput.begin() + index, parsedInput.end());
            dup2(fd, 0);
            break;
        }
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
