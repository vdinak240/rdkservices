#include <iostream>
#include <thread>
#include <fstream>
#include <chrono>
#include <cstdint>  // For uint16_t
#include <unistd.h> 
int main(int argc, char* argv[]) {

        // Define the output file path (adjust as needed)
    const std::string filename = "child_args.txt";
    
    // Open the file for writing (overwrite if exists)
    std::ofstream outfile(filename);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing." << std::endl;
        return 1;  // Exit with error code
    }
    
    // Write the program name (argv[0]) and all arguments to the file
    outfile << "Program: " << argv[0] << std::endl;
    outfile << "Arguments:" << std::endl;
    for (int i = 1; i < argc; ++i) {  // Start from 1 to skip program name
        outfile << argv[i] << std::endl;  // One argument per line
    }
    
    // Close the file
    outfile.close();


    // Disable stdout buffering for immediate output through the pipe
    setbuf(stdout, NULL);
    // Array of standard keyboard characters (UTF-16 values for A-Z, a-z, 0-9, and common symbols)
    uint16_t keyboard_chars[] = {
        0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A,  // A-J
        0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054,  // K-T
        0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A,                                // U-Z
        0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A,  // a-j
        0x006B, 0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,  // k-t
        0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A,                                // u-z
        0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039,  // 0-9
        0x0021, 0x0040, 0x0023, 0x0024, 0x0025, 0x005E, 0x0026, 0x002A, 0x0028, 0x0029,  // !@#$%^&*()
        0x002D, 0x005F, 0x003D, 0x002B, 0x005B, 0x005D, 0x007B, 0x007D, 0x007C,          // -_=+[]{}|
        0x003B, 0x003A, 0x0027, 0x0022, 0x002C, 0x002E, 0x003C, 0x003E, 0x003F,          // ;:'",.<>
	0x000A,  // Enter key (newline)
        0x0008   // Backspace key
    };
    const int num_chars = sizeof(keyboard_chars) / sizeof(keyboard_chars[0]);
    int index = 0;
    // Infinite loop to send a different keyboard character every 1 second
    while (true) {
        // Get the current character from the array
        uint16_t utf16_char = keyboard_chars[index % num_chars];
        // Write raw UTF-16 bytes to stdout
        //std::cout.write(reinterpret_cast<const char*>(&utf16_char), sizeof(utf16_char));
        //std::cout.flush();  // Ensure output is sent immediately
	// Write raw UTF-16 bytes directly to stdout (unbuffered)
        write(1, reinterpret_cast<const char*>(&utf16_char), sizeof(utf16_char));
        // Sleep for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Move to the next character
        index++;
    }
    return 0;  // Unreachable
}

