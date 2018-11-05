#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>

#include <readline/readline.h>
#include <readline/history.h>

using namespace std;


void parseInput(string inputString, vector<string> &parsedInput);
void printMessage(string msg, int type);
void execIt(char** parsedInput);

int main(int argc, char *argv[]) {
    char* buf;
    vector<string> parsedInput;
    while ((buf = readline(">> ")) != nullptr) { // readline library
        parsedInput.clear();
        if (strlen(buf) > 0) {
            add_history(buf);
        
            //printf("[%s]\n", buf);
            
            // parse input for arguments
            parseInput(buf, parsedInput);
            for (auto& inputChunk : parsedInput) {
                //cout << inputChunk << endl;
            }
            if (parsedInput[0] == "cd") { // if cd command, chdir
                if  (chdir(parsedInput[1].c_str()) == -1) {
                    printMessage(parsedInput[0], 1);
                }
            }
            else {
                if (parsedInput.size() > 1) { // if there's more than one input chunk
                    if (parsedInput[1][0] == '$') { // if the second input chunk is a $VARIABLE
                        parsedInput[1] = parsedInput[1].substr(1, parsedInput[1].length() - 1);
                        parsedInput[1] = getenv(parsedInput[1].c_str());
                    }
                }
                switch (int id = fork()) {
                    case -1: { // failed fork
                        cout << "nop" << endl;
                        break;
                        
                    }
                    case 0: { // child
                        // exec
                        vector<char*> cParsedInput;
                        cParsedInput.reserve(parsedInput.size());
                        for(size_t i = 0; i < parsedInput.size(); ++i)
                            cParsedInput.push_back(const_cast<char*>(parsedInput[i].c_str()));
                        cParsedInput.push_back(NULL);
                        
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
    
    // attempt to exec with all possible paths
    string path = getenv("PATH");
    path += ": ";
    istringstream istream(path);
    string token;
    while (getline(istream, token, ':')) {
        execv((token + "/" + parsedInput[0]).c_str(), parsedInput);
    }
}
