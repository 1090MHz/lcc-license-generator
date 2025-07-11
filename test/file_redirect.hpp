#ifndef TEST_FILE_REDIRECT_HPP_
#define TEST_FILE_REDIRECT_HPP_

#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>

namespace license {
namespace test {

struct file_redirect {
    file_redirect() {
        // Create temporary file with unique name using timestamp
        std::srand(std::time(nullptr));
        int random_num = std::rand();
        
#ifdef _WIN32
        char* temp_dir = std::getenv("TEMP");
        if (!temp_dir) temp_dir = std::getenv("TMP");
        if (!temp_dir) temp_dir = "C:\\temp";
        temp_filename = std::string(temp_dir) + "\\licensecc_test_" + std::to_string(random_num) + ".txt";
#else
        temp_filename = "/tmp/licensecc_test_" + std::to_string(random_num) + ".txt";
#endif
        
        // Redirect cout to file
        temp_file.open(temp_filename.c_str());
        old_cout = std::cout.rdbuf(temp_file.rdbuf());
    }
    
    ~file_redirect() { 
        // Restore cout
        std::cout.rdbuf(old_cout);
        temp_file.close();
        
        // Clean up temp file
        std::remove(temp_filename.c_str());
    }
    
    std::string str() {
        // Flush and read file contents
        temp_file.flush();
        std::ifstream read_file(temp_filename.c_str());
        return std::string((std::istreambuf_iterator<char>(read_file)),
                          std::istreambuf_iterator<char>());
    }

private:
    std::string temp_filename;
    std::ofstream temp_file;
    std::streambuf* old_cout;
};

} // namespace test
} // namespace license

#endif // TEST_FILE_REDIRECT_HPP_
