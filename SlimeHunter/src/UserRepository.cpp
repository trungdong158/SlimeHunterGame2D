#include "UserRepository.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <ctime>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

const char* const kUsersHeader = "user_id,username,password,created_at";
const char* const kProgressHeader = "user_id,current_level,max_level_unlocked,player_max_hp,player_damage,total_kills,total_score,updated_at";

// Giới hạn hợp lý cho dữ liệu đọc từ file (chống file bị sửa bậy làm game hỏng)
const int kMaxLevelValue = 999;
const int kMaxStat = 100000;
const int kMaxCounter = 1000000000;

std::string nowString() {
    const std::time_t t = std::time(nullptr);
    const std::tm* local = std::localtime(&t);
    char buffer[32] = "1970-01-01 00:00:00";
    if (local != nullptr) std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local);
    return buffer;
}

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

int clampInt(int value, int lo, int hi) { return value < lo ? lo : (value > hi ? hi : value); }

// Thay file đích bằng file tạm. Windows/MinGW đôi khi không cho rename đè nên có các phương án dự phòng.
bool replaceFile(const fs::path& temp, const fs::path& dest) {
    std::error_code ec;
    fs::rename(temp, dest, ec);
    if (!ec) return true;
    ec.clear();
    fs::remove(dest, ec);
    ec.clear();
    fs::rename(temp, dest, ec);
    if (!ec) return true;
    ec.clear();
    fs::copy_file(temp, dest, fs::copy_options::overwrite_existing, ec);
    const bool copied = !ec;
    ec.clear();
    fs::remove(temp, ec);
    return copied;
}

}  // namespace

// ============================================================================
//  Tiện ích CSV
// ============================================================================
namespace csv {

std::string escapeField(const std::string& field) {
    if (field.find_first_of(",\"\r\n") == std::string::npos) return field;
    std::string out = "\"";
    for (char c : field) {
        if (c == '"') out += "\"\"";  // ngoặc kép được nhân đôi
        else out += c;
    }
    out += '"';
    return out;
}

std::vector<std::string> splitLine(const std::string& line, bool* wellFormed) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;
    bool ok = true;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current += c;
            }
        } else if (c == '"') {
            if (current.empty()) inQuotes = true;
            else ok = false;  // ngoặc kép giữa một trường không được bọc: định dạng sai
        } else if (c == ',') {
            fields.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (inQuotes) ok = false;  // thiếu ngoặc kép đóng
    fields.push_back(current);
    if (wellFormed != nullptr) *wellFormed = ok;
    return fields;
}

bool parseInt(const std::string& text, int& out) {
    if (text.empty()) return false;
    int value = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end) return false;  // không phải số, hoặc còn ký tự thừa, hoặc tràn số
    out = value;
    return true;
}

}  // namespace csv

// ============================================================================
//  Khởi tạo và hạ tầng đọc/ghi file
// ============================================================================
fs::path UserRepository::findDataDirectory() {
    std::vector<fs::path> candidates = {"data", "../data", "../../data"};
#ifdef SLIMEHUNTER_SOURCE_DATA_DIR
    candidates.emplace_back(SLIMEHUNTER_SOURCE_DATA_DIR);
#endif
    for (const fs::path& candidate : candidates) {
        std::error_code ec;
        if (fs::is_directory(candidate, ec)) return candidate;
    }
    return "data";  // chưa có: constructor sẽ tạo
}

UserRepository::UserRepository(fs::path dataDir) : m_dataDir(std::move(dataDir)) {
    std::error_code ec;
    fs::create_directories(m_dataDir, ec);
    if (ec) {
        fail("Khong tao duoc thu muc du lieu: " + m_dataDir.string());
        return;
    }
    m_usable = ensureFile(usersPath(), kUsersHeader) && ensureFile(progressPath(), kProgressHeader);
}

void UserRepository::fail(const std::string& message) const { m_lastError = message; }

bool UserRepository::ensureFile(const fs::path& path, const std::string& header) {
    std::error_code ec;
    if (fs::exists(path, ec) && fs::file_size(path, ec) > 0) return true;  // đã có nội dung
    std::ofstream out(path, std::ios::trunc);                              // chưa có hoặc rỗng: ghi dòng tiêu đề
    if (!out) {
        fail("Khong tao duoc file: " + path.string());
        return false;
    }
    out << header << '\n';
    out.flush();
    if (!out) {
        fail("Khong ghi duoc file: " + path.string());
        return false;
    }
    return true;
}

bool UserRepository::readLines(const fs::path& path, std::vector<std::string>& lines) const {
    lines.clear();
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) {  // bỏ BOM UTF-8 nếu file được lưu bằng Notepad/Excel
            first = false;
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF)
                line.erase(0, 3);
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();  // file kiểu Windows (CRLF)
        lines.push_back(line);
    }
    return true;
}

bool UserRepository::writeLinesAtomically(const fs::path& path, const std::vector<std::string>& lines) {
    // Ghi ra file tạm rồi mới thay thế file thật: nếu máy tắt giữa chừng, file cũ vẫn nguyên vẹn.
    fs::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            fail("Khong mo duoc file tam de ghi: " + temp.string());
            return false;
        }
        for (const std::string& line : lines) out << line << '\n';
        out.flush();
        if (!out) {
            fail("Loi khi ghi file tam: " + temp.string());
            return false;
        }
    }
    if (!replaceFile(temp, path)) {
        fail("Khong thay the duoc file: " + path.string());
        return false;
    }
    return true;
}

// ============================================================================
//  Mật khẩu (tách riêng để có thể chuyển sang hash)
// ============================================================================
std::string UserRepository::encodePassword(const std::string& plain) { return plain; }

bool UserRepository::verifyPassword(const std::string& plain, const std::string& stored) { return encodePassword(plain) == stored; }

// ============================================================================
//  Tài khoản
// ============================================================================
bool UserRepository::isValidUsername(const std::string& username) {
    if (username.size() < 3 || username.size() > 20) return false;
    for (unsigned char c : username)
        if (!(std::isalnum(c) && c < 128) && c != '_' && c != '-') return false;
    return true;
}

bool UserRepository::isValidPassword(const std::string& password) {
    if (password.size() < 4 || password.size() > 32) return false;
    for (unsigned char c : password)
        if (c <= 32 || c >= 127 || c == ',' || c == '"') return false;
    return true;
}

std::vector<UserRecord> UserRepository::readAllUsers() const {
    std::vector<UserRecord> users;
    std::vector<std::string> lines;
    if (!readLines(usersPath(), lines)) return users;
    for (std::size_t i = 1; i < lines.size(); ++i) {  // lines[0] là tiêu đề
        if (lines[i].empty()) continue;
        bool wellFormed = true;
        const std::vector<std::string> f = csv::splitLine(lines[i], &wellFormed);
        UserRecord u;
        if (!wellFormed || f.size() < 4 || !csv::parseInt(f[0], u.userId) || u.userId <= 0 || !isValidUsername(f[1]) || f[2].empty() ||
            f[2].size() > 128) {
            m_warnings.push_back("users.csv: bo qua dong " + std::to_string(i + 1) + " (sai dinh dang)");  // KHÔNG in nội dung dòng (có mật khẩu)
            continue;
        }
        u.username = f[1];
        u.password = f[2];
        u.createdAt = f[3];
        users.push_back(std::move(u));
    }
    return users;
}

std::optional<UserRecord> UserRepository::findByUsername(const std::string& username) const {
    const std::string wanted = toLower(username);
    for (UserRecord& u : readAllUsers())
        if (toLower(u.username) == wanted) return u;
    return std::nullopt;
}

bool UserRepository::usernameExists(const std::string& username) const { return findByUsername(username).has_value(); }

AuthResult UserRepository::registerUser(const std::string& username, const std::string& password, UserRecord* created) {
    if (!isValidUsername(username)) return AuthResult::InvalidUsername;
    if (!isValidPassword(password)) return AuthResult::InvalidPassword;
    if (!m_usable) return AuthResult::StorageError;

    const std::vector<UserRecord> users = readAllUsers();
    int nextId = 1;
    const std::string wanted = toLower(username);
    for (const UserRecord& u : users) {
        if (toLower(u.username) == wanted) return AuthResult::UsernameTaken;  // không phân biệt hoa/thường
        nextId = std::max(nextId, u.userId + 1);
    }

    // Nếu file không kết thúc bằng xuống dòng (bị sửa tay) thì thêm '\n' trước khi nối bản ghi mới.
    bool needNewline = false;
    {
        std::ifstream in(usersPath(), std::ios::binary | std::ios::ate);
        if (in && in.tellg() > 0) {
            in.seekg(-1, std::ios::end);
            char last = '\n';
            in.get(last);
            needNewline = (last != '\n');
        }
    }
    std::ofstream out(usersPath(), std::ios::app);  // nối thêm vào cuối: các tài khoản cũ không bị đụng tới
    if (!out) {
        fail("Khong mo duoc users.csv de ghi");
        return AuthResult::StorageError;
    }
    if (needNewline) out << '\n';
    out << nextId << ',' << csv::escapeField(username) << ',' << csv::escapeField(encodePassword(password)) << ',' << nowString() << '\n';
    out.flush();
    if (!out) {
        fail("Loi khi ghi users.csv");
        return AuthResult::StorageError;
    }
    out.close();

    if (created != nullptr) {
        created->userId = nextId;
        created->username = username;
        created->password.clear();
        created->createdAt = nowString();
    }
    saveProgress(defaultProgress(nextId));  // tạo luôn dòng tiến trình mặc định (nếu lỗi thì lúc đăng nhập sẽ dùng mặc định)
    return AuthResult::Ok;
}

AuthResult UserRepository::login(const std::string& username, const std::string& password, UserRecord& userOut) {
    if (!isValidUsername(username) || password.empty()) return AuthResult::WrongCredentials;
    const std::optional<UserRecord> user = findByUsername(username);
    // Cùng một kết quả cho "không có tài khoản" và "sai mật khẩu" để không tiết lộ tài khoản nào tồn tại.
    if (!user.has_value() || !verifyPassword(password, user->password)) return AuthResult::WrongCredentials;
    userOut = *user;
    userOut.password.clear();  // không giữ mật khẩu trong bộ nhớ lâu hơn cần thiết
    return AuthResult::Ok;
}

// ============================================================================
//  Tiến trình
// ============================================================================
ProgressRecord UserRepository::defaultProgress(int userId) {
    ProgressRecord p;
    p.userId = userId;
    p.updatedAt = nowString();
    return p;
}

bool UserRepository::isValidProgress(const ProgressRecord& p) {
    return p.userId > 0 && p.currentLevel >= 1 && p.currentLevel <= kMaxLevelValue && p.maxLevelUnlocked >= 1 &&
           p.maxLevelUnlocked <= kMaxLevelValue && p.currentLevel <= p.maxLevelUnlocked && p.playerMaxHp >= 1 && p.playerMaxHp <= kMaxStat &&
           p.playerDamage >= 1 && p.playerDamage <= kMaxStat && p.totalKills >= 0 && p.totalKills <= kMaxCounter && p.totalScore >= 0 &&
           p.totalScore <= kMaxCounter;
}

namespace {
bool parseProgressLine(const std::string& line, ProgressRecord& p) {
    bool wellFormed = true;
    const std::vector<std::string> f = csv::splitLine(line, &wellFormed);
    if (!wellFormed || f.size() < 8) return false;
    if (!csv::parseInt(f[0], p.userId) || !csv::parseInt(f[1], p.currentLevel) || !csv::parseInt(f[2], p.maxLevelUnlocked) ||
        !csv::parseInt(f[3], p.playerMaxHp) || !csv::parseInt(f[4], p.playerDamage) || !csv::parseInt(f[5], p.totalKills) ||
        !csv::parseInt(f[6], p.totalScore))
        return false;
    p.updatedAt = f[7];
    return UserRepository::isValidProgress(p);
}
}  // namespace

ProgressRecord UserRepository::loadProgress(int userId) const {
    std::vector<std::string> lines;
    if (readLines(progressPath(), lines)) {
        for (std::size_t i = 1; i < lines.size(); ++i) {
            if (lines[i].empty()) continue;
            const std::vector<std::string> f = csv::splitLine(lines[i]);
            int id = 0;
            if (f.empty() || !csv::parseInt(f[0], id) || id != userId) continue;  // dòng của người khác
            ProgressRecord p;
            if (parseProgressLine(lines[i], p)) return p;
            m_warnings.push_back("player_progress.csv: dong " + std::to_string(i + 1) + " bi hong, dung du lieu mac dinh");
            break;
        }
    }
    return defaultProgress(userId);
}

bool UserRepository::saveProgress(const ProgressRecord& input) {
    if (input.userId <= 0) {
        fail("userId khong hop le");
        return false;
    }
    ProgressRecord p = input;  // ép về giới hạn hợp lệ trước khi ghi để không bao giờ ghi ra dòng "hỏng"
    p.maxLevelUnlocked = clampInt(p.maxLevelUnlocked, 1, kMaxLevelValue);
    p.currentLevel = clampInt(p.currentLevel, 1, p.maxLevelUnlocked);
    p.playerMaxHp = clampInt(p.playerMaxHp, 1, kMaxStat);
    p.playerDamage = clampInt(p.playerDamage, 1, kMaxStat);
    p.totalKills = clampInt(p.totalKills, 0, kMaxCounter);
    p.totalScore = clampInt(p.totalScore, 0, kMaxCounter);
    p.updatedAt = nowString();

    const std::string newLine = std::to_string(p.userId) + ',' + std::to_string(p.currentLevel) + ',' + std::to_string(p.maxLevelUnlocked) + ',' +
                                std::to_string(p.playerMaxHp) + ',' + std::to_string(p.playerDamage) + ',' + std::to_string(p.totalKills) + ',' +
                                std::to_string(p.totalScore) + ',' + csv::escapeField(p.updatedAt);

    std::vector<std::string> lines;
    readLines(progressPath(), lines);  // file chưa có thì lines rỗng
    std::vector<std::string> out;
    out.push_back(kProgressHeader);
    bool replaced = false;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i == 0 && lines[i] == kProgressHeader) continue;  // tiêu đề đã thêm ở trên
        if (lines[i].empty()) continue;
        const std::vector<std::string> f = csv::splitLine(lines[i]);
        int id = 0;
        if (!f.empty() && csv::parseInt(f[0], id) && id == p.userId) {
            if (!replaced) {
                out.push_back(newLine);  // thay đúng dòng của người chơi này
                replaced = true;
            }                          // (dòng trùng user_id thứ hai nếu có sẽ bị loại)
        } else {
            out.push_back(lines[i]);   // dòng của người khác (kể cả dòng lạ): giữ NGUYÊN VĂN
        }
    }
    if (!replaced) out.push_back(newLine);
    return writeLinesAtomically(progressPath(), out);
}
