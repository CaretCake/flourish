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
void execIt(char** parsedInput);
//char *stringVecToCharArr (vector<string> parsedInputVector, char * &parsedInputArray);

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
                cout << "changing directory" << endl;
            }
            else {
                switch (int id = fork()) {
                    case -1: { // failed fork
                        cout << "nop" << endl;
                        break;
                        
                    }
                    case 0: { // child
                        // exec
                        int numInputChunks = parsedInput.size();
                        char *parsedInputArray[15];
                        for (int i = 0; i < parsedInput.size() - 1; i++) {
                            strcpy(parsedInputArray[i], parsedInput[i].c_str());
                        }
                        parsedInputArray[parsedInput.size()] = NULL;
                        execIt(parsedInputArray);
                        // failed exec
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

void execIt(char** parsedInput) {
    // attempt to execv on input for explicit file path
    execv(parsedInput[0], parsedInput);
    // attempt to exec with all possible paths
    string path = getenv("PATH");
    path += ": ";
    istringstream istream(path);
    string token;
    while (getline(istream, token, ':')) {
        cout << parsedInput << endl;
        execv((token + "/" + parsedInput[0]).c_str(), parsedInput);
    }
}

/*char *stringVecToCharArr (vector<string> parsedInputVector, char ** &parsedInputArray) {
    for(size_t i = 0; i < parsedInputVector.size(); i++){
        parsedInputArray[i] = new char[parsedInputVector[i].size() + 1];
        strcpy(parsedInputArray[i], parsedInputVector[i].c_str());
    }
    return parsedInputArray;
}*/
