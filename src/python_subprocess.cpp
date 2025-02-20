#include <zinc/python.hpp>

#include <iostream>
#include <cstdio>
#include <memory>
#include <filesystem>
#include <cstdlib>

std::string_view zinc::Python::execute(std::string_view script) {
    // Create a temporary file to hold the Python script
    std::string tmpFile = "tmp.py";
    FILE* fp = fopen(tmpFile.c_str(), "w");
    if (fp == NULL) {
        return "Error: unable to create temporary file";
    }
    fwrite(script.data(), 1, script.size(), fp);
    fclose(fp);

    // Execute the Python script using subprocess
    std::string command = "/usr/bin/env python3 " + tmpFile;
    static thread_local std::string output;
    output.clear();
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == NULL) {
        return "Error: unable to execute Python script";
    }
    char buffer[128];
    while (fgets(buffer, 128, pipe) != NULL) {
        output += buffer;
    }
    pclose(pipe);

    // Remove the temporary file
    remove(tmpFile.c_str());

    return output;
}
