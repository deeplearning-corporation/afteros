// FileManager.cpp - AfterOS 文件管理器（类似资源管理器）
#include <iostream>
#include <vector>
#include <string>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/stat.h>
#endif

// 颜色定义
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_GRAY    "\033[90m"

// 文件信息结构
struct FileInfo {
    std::string name;
    bool is_directory;
    size_t size;
    time_t modify_time;
    std::string extension;
};

class FileManager {
private:
    std::string current_path;
    std::vector<FileInfo> entries;
    int selected_index;
    int scroll_offset;
    std::string status_message;
    int message_timeout;
    std::string clipboard_path;
    bool clipboard_cut;

    // 获取文件扩展名
    std::string getExtension(const std::string& filename) {
        size_t dot_pos = filename.find_last_of('.');
        if (dot_pos != std::string::npos && dot_pos != 0) {
            std::string ext = filename.substr(dot_pos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            return ext;
        }
        return "";
    }

    // 检查是否为目录
    bool isDirectory(const std::string& path) {
        DIR* dir = opendir(path.c_str());
        if (dir) {
            closedir(dir);
            return true;
        }
        return false;
    }

    // 获取文件大小
    size_t getFileSize(const std::string& path) {
        std::ifstream file(path.c_str(), std::ifstream::ate | std::ifstream::binary);
        if (!file.is_open()) return 0;
        return file.tellg();
    }

    // 获取修改时间
    time_t getModifyTime(const std::string& path) {
        // 简化版本，返回当前时间
        return time(nullptr);
    }

    // 格式化文件大小
    std::string formatSize(size_t size) {
        if (size == 0) return "空";

        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int unit_index = 0;
        double file_size = size;

        while (file_size >= 1024 && unit_index < 4) {
            file_size /= 1024;
            unit_index++;
        }

        char buffer[32];
        if (unit_index == 0) {
            snprintf(buffer, sizeof(buffer), "%.0f %s", file_size, units[unit_index]);
        }
        else {
            snprintf(buffer, sizeof(buffer), "%.1f %s", file_size, units[unit_index]);
        }
        return std::string(buffer);
    }

    // 获取文件图标
    std::string getFileIcon(const FileInfo& file) {
        if (file.is_directory) {
            if (file.name == "..") return "📁 ";
            return "📂 ";
        }

        std::string ext = file.extension;
        if (ext == "txt" || ext == "md" || ext == "log") return "📄 ";
        if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" || ext == "bmp") return "🖼️ ";
        if (ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "flac") return "🎵 ";
        if (ext == "mp4" || ext == "avi" || ext == "mkv" || ext == "mov") return "🎬 ";
        if (ext == "pdf") return "📕 ";
        if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz") return "🗜️ ";
        if (ext == "exe" || ext == "elf" || ext == "bin") return "⚙️ ";
        if (ext == "sh" || ext == "py" || ext == "c" || ext == "cpp" || ext == "js") return "💻 ";
        if (ext == "html" || ext == "htm" || ext == "css") return "🌐 ";
        if (ext == "doc" || ext == "docx") return "📝 ";
        if (ext == "xls" || ext == "xlsx") return "📊 ";

        return "📄 ";
    }

    // 获取文件颜色
    std::string getFileColor(const FileInfo& file) {
        if (file.is_directory) {
            if (file.name == "..") return COLOR_GRAY;
            return COLOR_BLUE;
        }

        std::string ext = file.extension;
        if (ext == "txt" || ext == "md") return COLOR_WHITE;
        if (ext == "jpg" || ext == "png" || ext == "gif") return COLOR_MAGENTA;
        if (ext == "mp3" || ext == "wav") return COLOR_GREEN;
        if (ext == "sh" || ext == "py" || ext == "cpp") return COLOR_YELLOW;
        if (ext == "elf" || ext == "bin") return COLOR_RED;
        if (ext == "zip" || ext == "rar") return COLOR_CYAN;

        return COLOR_WHITE;
    }

    // 加载目录内容
    void loadDirectory() {
        entries.clear();

        // 添加返回上一级
        FileInfo parent;
        parent.name = "..";
        parent.is_directory = true;
        parent.size = 0;
        parent.extension = "";
        entries.push_back(parent);

        DIR* dir = opendir(current_path.c_str());
        if (dir == nullptr) {
            setStatusMessage("无法打开目录: " + current_path, true);
            return;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name != "." && name != "..") {
                FileInfo file;
                file.name = name;
                file.extension = getExtension(name);

                std::string full_path = current_path + "/" + name;
                file.is_directory = isDirectory(full_path);

                if (!file.is_directory) {
                    file.size = getFileSize(full_path);
                }
                else {
                    file.size = 0;
                }

                file.modify_time = getModifyTime(full_path);
                entries.push_back(file);
            }
        }

        closedir(dir);

        // 排序：目录在前，文件在后，按名称排序
        std::sort(entries.begin() + 1, entries.end(),
            [](const FileInfo& a, const FileInfo& b) {
                if (a.is_directory != b.is_directory) {
                    return a.is_directory > b.is_directory;
                }
                return a.name < b.name;
            });

        // 确保选中索引有效
        if (selected_index >= (int)entries.size()) {
            selected_index = entries.size() - 1;
        }
        if (selected_index < 0) {
            selected_index = 0;
        }

        // 调整滚动位置
        if (selected_index < scroll_offset) {
            scroll_offset = selected_index;
        }
        else if (selected_index >= scroll_offset + 20) {
            scroll_offset = selected_index - 19;
        }

        setStatusMessage("已加载 " + std::to_string(entries.size() - 1) + " 个项目");
    }

    // 显示顶部栏
    void displayHeader() {
        std::cout << COLOR_BOLD << COLOR_CYAN;
        std::cout << "╔════════════════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║";
        std::cout << COLOR_YELLOW << " AfterOS 文件管理器 ";
        std::cout << COLOR_CYAN << "│ ";
        std::cout << COLOR_GREEN << "当前位置: ";
        std::cout << COLOR_WHITE << current_path;

        // 填充空格
        int spaces = 72 - (20 + current_path.length());
        if (spaces > 0) {
            std::cout << std::string(spaces, ' ');
        }

        std::cout << COLOR_CYAN << "║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════════════════════════╣" << std::endl;

        // 列标题
        std::cout << "║ ";
        std::cout << COLOR_MAGENTA << " 名称" << std::string(35, ' ');
        std::cout << "大小" << std::string(10, ' ');
        std::cout << "类型" << std::string(10, ' ');
        std::cout << COLOR_CYAN << "║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════════════════════════╣" << std::endl;
        std::cout << COLOR_RESET;
    }

    // 显示文件列表
    void displayFileList() {
        int display_count = 0;
        int max_display = 25;

        for (size_t i = scroll_offset; i < entries.size() && display_count < max_display; i++) {
            const FileInfo& file = entries[i];

            // 高亮选中的行
            if ((int)i == selected_index) {
                std::cout << COLOR_BOLD << COLOR_YELLOW << "║▶ ";
            }
            else {
                std::cout << "║  ";
            }

            // 显示图标
            std::cout << getFileIcon(file);

            // 显示文件名（限制长度）
            std::string display_name = file.name;
            if (display_name.length() > 38) {
                display_name = display_name.substr(0, 35) + "...";
            }

            std::cout << getFileColor(file);
            printf("%-38s", display_name.c_str());

            // 显示大小
            std::cout << COLOR_GREEN;
            if (!file.is_directory && file.size > 0) {
                printf("%-12s", formatSize(file.size).c_str());
            }
            else if (file.is_directory && file.name != "..") {
                printf("%-12s", "<目录>");
            }
            else {
                printf("%-12s", "-");
            }

            // 显示类型
            std::cout << COLOR_CYAN;
            if (file.is_directory) {
                if (file.name == "..") {
                    printf("%-10s", "上级目录");
                }
                else {
                    printf("%-10s", "文件夹");
                }
            }
            else {
                std::string type = file.extension.empty() ? "文件" : file.extension;
                if (type.length() > 10) type = type.substr(0, 8) + "..";
                printf("%-10s", type.c_str());
            }

            std::cout << COLOR_RESET;

            // 如果是目录，添加标记
            if (file.is_directory && file.name != "..") {
                std::cout << " 📁";
            }

            std::cout << std::endl;
            display_count++;
        }

        // 填充剩余行
        for (int i = display_count; i < max_display; i++) {
            std::cout << "║" << std::string(78, ' ') << "║" << std::endl;
        }
    }

    // 显示底部状态栏
    void displayFooter() {
        std::cout << COLOR_CYAN << "╠════════════════════════════════════════════════════════════════════════════╣" << std::endl;

        // 状态信息
        std::cout << "║ ";
        if (!status_message.empty()) {
            std::cout << COLOR_YELLOW << status_message;
            if (message_timeout > 0) {
                message_timeout--;
                if (message_timeout == 0) status_message.clear();
            }
        }
        else {
            std::cout << COLOR_GREEN << "就绪";
        }

        int spaces = 78 - (status_message.empty() ? 6 : status_message.length());
        if (spaces > 0) std::cout << std::string(spaces, ' ');
        std::cout << COLOR_CYAN << "║" << std::endl;

        // 快捷键栏
        std::cout << "╠════════════════════════════════════════════════════════════════════════════╣" << std::endl;
        std::cout << "║ ";
        std::cout << COLOR_GREEN << "↑/↓/k/j" << COLOR_WHITE << ":移动  ";
        std::cout << COLOR_GREEN << "Enter" << COLOR_WHITE << ":打开  ";
        std::cout << COLOR_GREEN << "F2" << COLOR_WHITE << ":重命名  ";
        std::cout << COLOR_GREEN << "Del" << COLOR_WHITE << ":删除  ";
        std::cout << COLOR_GREEN << "F5" << COLOR_WHITE << ":刷新  ";
        std::cout << COLOR_GREEN << "F7" << COLOR_WHITE << ":新建文件夹  ";
        std::cout << COLOR_GREEN << "Ctrl+C" << COLOR_WHITE << ":复制  ";
        std::cout << COLOR_GREEN << "Ctrl+X" << COLOR_WHITE << ":剪切  ";
        std::cout << COLOR_GREEN << "Ctrl+V" << COLOR_WHITE << ":粘贴  ";
        std::cout << COLOR_GREEN << "Q" << COLOR_WHITE << ":退出" << std::endl;

        std::cout << COLOR_CYAN << "╚════════════════════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << COLOR_RESET;
        std::cout << "> ";
        std::cout.flush();
    }

    // 显示界面
    void display() {
        std::cout << "\033[2J\033[1;1H";
        displayHeader();
        displayFileList();
        displayFooter();
    }

    // 设置状态消息
    void setStatusMessage(const std::string& msg, bool is_error = false) {
        status_message = msg;
        message_timeout = 3;
        if (is_error) {
            status_message = "❌ " + msg;
        }
        else {
            status_message = "✓ " + msg;
        }
    }

    // 打开文件或目录
    void openCurrent() {
        if (selected_index >= 0 && selected_index < (int)entries.size()) {
            const FileInfo& file = entries[selected_index];
            std::string new_path = current_path + "/" + file.name;

            if (file.is_directory) {
                if (file.name == "..") {
                    // 返回上级目录
                    if (current_path == "/" || current_path.empty()) {
                        current_path = "/";
                    }
                    else {
                        size_t pos = current_path.find_last_of('/');
                        if (pos != std::string::npos) {
                            if (pos == 0) {
                                current_path = "/";
                            }
                            else {
                                current_path = current_path.substr(0, pos);
                            }
                        }
                    }
                }
                else {
                    current_path = new_path;
                }
                selected_index = 0;
                scroll_offset = 0;
                loadDirectory();
            }
            else {
                // 显示文件内容
                displayFileContent(new_path);
            }
        }
    }

    // 显示文件内容
    void displayFileContent(const std::string& path) {
        std::cout << "\033[2J\033[1;1H";
        std::cout << COLOR_CYAN << "╔════════════════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║ " << COLOR_YELLOW << "文件内容: " << COLOR_WHITE << path << std::endl;
        std::cout << COLOR_CYAN << "╠════════════════════════════════════════════════════════════════════════════╣" << std::endl;

        std::ifstream file(path.c_str());
        if (file.is_open()) {
            std::string line;
            int line_num = 1;
            int line_count = 0;

            while (std::getline(file, line) && line_count < 30) {
                std::cout << COLOR_MAGENTA << std::setw(4) << line_num++ << " " << COLOR_RESET << line << std::endl;
                line_count++;
            }
            file.close();

            if (line_num == 1) {
                std::cout << COLOR_RED << "  文件为空" << COLOR_RESET << std::endl;
            }
        }
        else {
            std::cout << COLOR_RED << "  无法显示文件内容（可能是二进制文件）" << COLOR_RESET << std::endl;
        }

        std::cout << COLOR_CYAN << "╚════════════════════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << COLOR_GREEN << "按 Enter 键继续..." << COLOR_RESET;
        std::cin.get();
    }

    // 删除文件或目录
    void deleteCurrent() {
        if (selected_index < 0 || selected_index >= (int)entries.size()) return;

        const FileInfo& file = entries[selected_index];
        if (file.name == "..") {
            setStatusMessage("无法删除上级目录", true);
            return;
        }

        std::string full_path = current_path + "/" + file.name;

        std::cout << "\n";
        std::cout << COLOR_YELLOW << "确认删除 " << file.name << " ？" << std::endl;
        std::cout << "按 Y 确认，其他键取消: " << COLOR_RESET;

        char confirm;
        std::cin >> confirm;
        std::cin.ignore();

        if (confirm == 'y' || confirm == 'Y') {
            bool success = false;
            if (file.is_directory) {
                success = (rmdir(full_path.c_str()) == 0);
            }
            else {
                success = (unlink(full_path.c_str()) == 0);
            }

            if (success) {
                setStatusMessage("已删除: " + file.name);
                loadDirectory();
            }
            else {
                setStatusMessage("删除失败: " + file.name, true);
            }
        }
    }

    // 重命名文件或目录
    void renameCurrent() {
        if (selected_index < 0 || selected_index >= (int)entries.size()) return;

        const FileInfo& file = entries[selected_index];
        if (file.name == "..") {
            setStatusMessage("无法重命名上级目录", true);
            return;
        }

        std::string old_path = current_path + "/" + file.name;
        std::string new_name;

        std::cout << "\n";
        std::cout << COLOR_YELLOW << "重命名: " << file.name << std::endl;
        std::cout << "新名称: " << COLOR_RESET;
        std::cin.ignore();
        std::getline(std::cin, new_name);

        if (!new_name.empty() && new_name != file.name) {
            std::string new_path = current_path + "/" + new_name;
            if (rename(old_path.c_str(), new_path.c_str()) == 0) {
                setStatusMessage("已重命名为: " + new_name);
                loadDirectory();
            }
            else {
                setStatusMessage("重命名失败", true);
            }
        }
    }

    // 创建目录
    void createDirectory() {
        std::string dir_name;

        std::cout << "\n";
        std::cout << COLOR_YELLOW << "新建文件夹" << std::endl;
        std::cout << "文件夹名称: " << COLOR_RESET;
        std::cin.ignore();
        std::getline(std::cin, dir_name);

        if (!dir_name.empty()) {
            std::string full_path = current_path + "/" + dir_name;
#ifdef _WIN32
            if (mkdir(full_path.c_str()) == 0) {
#else
            if (mkdir(full_path.c_str(), 0755) == 0) {
#endif
                setStatusMessage("已创建文件夹: " + dir_name);
                loadDirectory();
            }
            else {
                setStatusMessage("创建文件夹失败", true);
            }
            }
        }

    // 复制/剪切
    void copyCurrent(bool cut) {
        if (selected_index < 0 || selected_index >= (int)entries.size()) return;

        const FileInfo& file = entries[selected_index];
        if (file.name == "..") {
            setStatusMessage("无法操作上级目录", true);
            return;
        }

        clipboard_path = current_path + "/" + file.name;
        clipboard_cut = cut;

        if (cut) {
            setStatusMessage("已剪切: " + file.name);
        }
        else {
            setStatusMessage("已复制: " + file.name);
        }
    }

    // 粘贴
    void paste() {
        if (clipboard_path.empty()) {
            setStatusMessage("剪贴板为空", true);
            return;
        }

        std::string dest_path = current_path + "/" +
            clipboard_path.substr(clipboard_path.find_last_of('/') + 1);

        if (clipboard_cut) {
            // 移动文件
            if (rename(clipboard_path.c_str(), dest_path.c_str()) == 0) {
                setStatusMessage("已移动: " + clipboard_path);
                clipboard_path.clear();
                loadDirectory();
            }
            else {
                setStatusMessage("移动失败", true);
            }
        }
        else {
            // 复制文件（简化版，只复制小文件）
            std::ifstream src(clipboard_path.c_str(), std::ios::binary);
            std::ofstream dst(dest_path.c_str(), std::ios::binary);

            if (src && dst) {
                dst << src.rdbuf();
                setStatusMessage("已复制: " + clipboard_path);
                loadDirectory();
            }
            else {
                setStatusMessage("复制失败", true);
            }
        }
    }

    // 刷新
    void refresh() {
        loadDirectory();
        setStatusMessage("已刷新");
    }

public:
    FileManager() : current_path("/"), selected_index(0), scroll_offset(0), message_timeout(0), clipboard_cut(false) {
        loadDirectory();
    }

    void run() {
        int input;
        bool running = true;

        while (running) {
            display();
            input = std::cin.get();

            // 处理功能键（简化处理）
            if (input == 27) { // ESC
                input = std::cin.get();
                if (input == 91) { // [
                    input = std::cin.get();
                    switch (input) {
                    case 65: // 上箭头
                        if (selected_index > 0) selected_index--;
                        break;
                    case 66: // 下箭头
                        if (selected_index < (int)entries.size() - 1) selected_index++;
                        break;
                    }
                }
            }
            else {
                switch (input) {
                case 'w': case 'W': case 'k': case 'K':
                    if (selected_index > 0) selected_index--;
                    break;

                case 's': case 'S': case 'j': case 'J':
                    if (selected_index < (int)entries.size() - 1) selected_index++;
                    break;

                case '\n': case '\r':
                    openCurrent();
                    break;

                case 127: case 8: // Delete
                    deleteCurrent();
                    break;

                case 19: // Ctrl+S
                    break;

                case 3: // Ctrl+C
                    copyCurrent(false);
                    break;

                case 24: // Ctrl+X
                    copyCurrent(true);
                    break;

                case 22: // Ctrl+V
                    paste();
                    break;

                case 'r': case 'R':
                    refresh();
                    break;

                case 63: // F2
                case '2':
                    renameCurrent();
                    break;

                case '7':
                    createDirectory();
                    break;

                case 'q': case 'Q':
                    running = false;
                    break;
                }
            }

            // 自动调整滚动
            if (selected_index < scroll_offset) {
                scroll_offset = selected_index;
            }
            else if (selected_index >= scroll_offset + 24) {
                scroll_offset = selected_index - 23;
            }
        }

        std::cout << "\033[2J\033[1;1H";
        std::cout << COLOR_GREEN << "感谢使用 AfterOS 文件管理器！" << COLOR_RESET << std::endl;
    }
    };

int main(int argc, char* argv[]) {
    FileManager fm;

    if (argc > 1) {
        // 可以设置起始路径
    }

    fm.run();
    return 0;
}
