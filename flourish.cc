#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <regex>

#include <readline/readline.h>
#include <readline/history.h>

using namespace std;


void parseInput(string inputString, vector<string> &parsedInput);
vector<string> splitString(char delimiter, string target);
void printMessage(string msg, int type);
void execIt(char** parsedInput);

const char *DEFAULT_PROMPT =  ">> ";
const char *PROMPT = "PROMPT";

int main(int argc, char *argv[]) {
    char* buf;
    vector<string> parsedInput;
    
    if (!getenv(PROMPT)) {
        setenv(PROMPT, DEFAULT_PROMPT, 1);
    }
    
    while ((buf = readline(getenv("PROMPT"))) != nullptr) { // readline library
        parsedInput.clear();
        if (strlen(buf) > 0) {
            add_history(buf);
        
            //printf("[%s]\n", buf);
            
            // parse input for arguments
            parseInput(buf, parsedInput);
            for (auto& inputChunk : parsedInput) {
                //cout << inputChunk << endl;
            }
            // check for environment variables
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
                int tildeReplaceLocation = parsedInput[i].find("~");
                if (tildeReplaceLocation != string::npos) {
                    cout << "found a ~! at: " << tildeReplaceLocation << endl;
                    parsedInput[i].erase(tildeReplaceLocation, 1);
                    parsedInput[i].insert(tildeReplaceLocation, getenv("HOME"));
                    cout << "now: " << parsedInput[i] << endl;
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
                        cout << "nop" << endl;
                        break;
                        
                    }
                    case 0: { // child
                        // dup must happen here
                        // if ">"
                            // dup2
                            // remove rest from the arg list
                        // if "<"
                            // dup2
                            // remove rest from the arg list
                        
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
        free(buf);
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
            cout << msg << ": command not found" << endl;
            break;
        }
        case 1: { // bad chdir
            cout << msg << ": no such directory" << endl;
            break;
        }
    }
}

void execIt(char** parsedInput) {
    // attempt to execv on input for explicit file path
    execv(parsedInput[0], parsedInput);
    
    vector<string> paths = splitString(':', getenv("PATH"));
    for (const auto& path : paths) {
        execv((path + "/" + parsedInput[0]).c_str(), parsedInput);
    }
}
