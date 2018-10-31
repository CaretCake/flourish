#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>

#include <readline/readline.h>
#include <readline/history.h>

using namespace std;

void parseInput(string inputString, vector<string> &parsedInput);
void execIt();

int main(int argc, char *argv[]) {
    char* buf;
    while ((buf = readline(">> ")) != nullptr) { // readline library
        if (strlen(buf) > 0) {
            add_history(buf);
        
        printf("[%s]\n", buf);
        
        // parse input for arguments
        // check if cd command
            // chdir
        // else
            // fork
                // if kid
                    // exec
                    // failed exec
                // if parent
                    // fork
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

void execIt() {
    // for path in PATH
        // execv(path + "/" + arg[0], arg)
}
