#include "xconfig.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace cfg
{
    namespace
    {
        //---------------------------------------------------------------------
        // Errors

        void abort_on_error(const char* message)
        {
            std::fprintf(stderr, "%s\n", message);
            std::abort();
        }

        error_handler g_error_handler = abort_on_error;

        //---------------------------------------------------------------------
        // Text handling
        //
        // std::isspace and friends take an int that must be representable as an
        // unsigned char; a plain char is signed here, so every call goes through
        // these rather than passing a possibly negative byte straight in.

        bool is_space(char c)
        {
            return std::isspace((unsigned char)c) != 0;
        }

        char lowered(char c)
        {
            return (char)std::tolower((unsigned char)c);
        }

        std::string lowered(const std::string& text)
        {
            std::string result = text;
            for (char& c : result)
            {
                c = lowered(c);
            }

            return result;
        }

        //---------------------------------------------------------------------
        // Trims whitespace off both ends. A carriage return counts as
        // whitespace, which is what makes a file saved with Windows line
        // endings read the same as one saved without.
        std::string trim(const std::string& text)
        {
            size_t first = 0;
            while (first < text.size() && is_space(text[first]))
            {
                ++first;
            }

            size_t last = text.size();
            while (last > first && is_space(text[last - 1]))
            {
                --last;
            }

            return text.substr(first, last - first);
        }

        //---------------------------------------------------------------------
        // Everything from the first comment marker on is dropped, so a value
        // can carry a note beside it. It also means a value cannot contain a
        // ';' or a '#' - no setting wants to, and the alternative is quoting
        // rules nobody would remember.
        std::string strip_comment(const std::string& line)
        {
            const size_t marker = line.find_first_of(";#");
            if (marker == std::string::npos)
            {
                return line;
            }

            return line.substr(0, marker);
        }

        //---------------------------------------------------------------------
        // A file saved as UTF-8 from Notepad starts with a byte order mark,
        // which would otherwise glue itself to the first section name and make
        // that whole section unfindable.
        void strip_byte_order_mark(std::string& line)
        {
            if (line.size() >= 3 &&
                (unsigned char)line[0] == 0xEF &&
                (unsigned char)line[1] == 0xBB &&
                (unsigned char)line[2] == 0xBF)
            {
                line.erase(0, 3);
            }
        }

        //---------------------------------------------------------------------
        // "section.key", or just the key for a setting written before any
        // header. Both halves are folded to lower case here, which is the only
        // place matching happens.
        std::string make_key(const std::string& section, const std::string& key)
        {
            if (section.empty())
            {
                return lowered(key);
            }

            return lowered(section) + "." + lowered(key);
        }

        //---------------------------------------------------------------------
        // How a setting reads back in an error message: "[walk] frame_hold",
        // and just the key when it was written before any header.
        std::string named(const std::string& section, const std::string& key)
        {
            if (section.empty())
            {
                return key;
            }

            return "[" + section + "] " + key;
        }

        //---------------------------------------------------------------------
        // Number parsing
        //
        // Both of these insist the whole value was consumed: "12abc" is a typo,
        // and reading it as 12 would hide that typo until something downstream
        // looked wrong.

        bool parse_long(const std::string& text, long& out)
        {
            if (text.empty())
            {
                return false;
            }

            char* end = nullptr;
            const long value = std::strtol(text.c_str(), &end, 10);

            if (end != text.c_str() + text.size())
            {
                return false;
            }

            out = value;

            return true;
        }

        bool parse_float(const std::string& text, float& out)
        {
            if (text.empty())
            {
                return false;
            }

            char* end = nullptr;
            const float value = std::strtof(text.c_str(), &end);

            if (end != text.c_str() + text.size())
            {
                return false;
            }

            out = value;

            return true;
        }
    }

    //-------------------------------------------------------------------------
    void set_error_handler(error_handler handler)
    {
        g_error_handler = (handler != nullptr) ? handler : abort_on_error;
    }

    //-------------------------------------------------------------------------
    bool ini::load(const std::string& path)
    {
        m_settings.clear();
        m_path = path;

        std::ifstream file(path);
        if (!file.is_open())
        {
            return false;
        }

        // Settings written before any [section] header keep the key on its own,
        // so get_*("", key) reaches them.
        std::string section;

        std::string line;
        bool first_line = true;

        while (std::getline(file, line))
        {
            if (first_line)
            {
                strip_byte_order_mark(line);
                first_line = false;
            }

            const std::string content = trim(strip_comment(line));
            if (content.empty())
            {
                continue;
            }

            if (content.front() == '[')
            {
                // An unterminated header is a typo; skipping it rather than
                // guessing keeps the keys under it out of the wrong section.
                if (content.back() != ']')
                {
                    continue;
                }

                section = trim(content.substr(1, content.size() - 2));

                continue;
            }

            const size_t separator = content.find('=');
            if (separator == std::string::npos)
            {
                continue;
            }

            const std::string key = trim(content.substr(0, separator));
            if (key.empty())
            {
                continue;
            }

            // A key given twice takes its last value, the way a later line
            // overriding an earlier one reads.
            m_settings[make_key(section, key)] = trim(content.substr(separator + 1));
        }

        return true;
    }

    //-------------------------------------------------------------------------
    const std::string* ini::find(const std::string& section, const std::string& key) const
    {
        const auto found = m_settings.find(make_key(section, key));
        if (found == m_settings.end())
        {
            return nullptr;
        }

        return &found->second;
    }

    //-------------------------------------------------------------------------
    void ini::report(const std::string& section, const std::string& key, const char* expected) const
    {
        const std::string* value = find(section, key);

        std::string message = m_path + ": " + named(section, key);

        if (value == nullptr)
        {
            message += " is not set";
        }
        else
        {
            message += " = " + *value + " is not " + expected;
        }

        g_error_handler(message.c_str());
    }

    //-------------------------------------------------------------------------
    bool ini::has(const std::string& section, const std::string& key) const
    {
        return find(section, key) != nullptr;
    }

    //-------------------------------------------------------------------------
    int ini::get_int(const std::string& section, const std::string& key) const
    {
        const std::string* value = find(section, key);

        long parsed = 0;
        if (value == nullptr || !parse_long(*value, parsed))
        {
            report(section, key, "a whole number");

            return 0;
        }

        return (int)parsed;
    }

    //-------------------------------------------------------------------------
    unsigned int ini::get_uint(const std::string& section, const std::string& key) const
    {
        const std::string* value = find(section, key);

        long parsed = 0;
        if (value == nullptr || !parse_long(*value, parsed) || parsed < 0)
        {
            // Worded as it is so a negative reads as the mistake it is rather
            // than wrapping around into an enormous count.
            report(section, key, "a whole number of zero or more");

            return 0;
        }

        return (unsigned int)parsed;
    }

    //-------------------------------------------------------------------------
    float ini::get_float(const std::string& section, const std::string& key) const
    {
        const std::string* value = find(section, key);

        float parsed = 0.0f;
        if (value == nullptr || !parse_float(*value, parsed))
        {
            report(section, key, "a number");

            return 0.0f;
        }

        return parsed;
    }

    //-------------------------------------------------------------------------
    bool ini::get_bool(const std::string& section, const std::string& key) const
    {
        const std::string* value = find(section, key);
        if (value != nullptr)
        {
            const std::string text = lowered(*value);

            if (text == "true" || text == "yes" || text == "on" || text == "1")
            {
                return true;
            }
            if (text == "false" || text == "no" || text == "off" || text == "0")
            {
                return false;
            }
        }

        report(section, key, "true or false");

        return false;
    }

    //-------------------------------------------------------------------------
    std::string ini::get_string(const std::string& section, const std::string& key) const
    {
        const std::string* value = find(section, key);
        if (value == nullptr)
        {
            report(section, key, "set");

            return std::string();
        }

        return *value;
    }

    //-------------------------------------------------------------------------
    const char* ini::get_cstr(const std::string& section, const std::string& key) const
    {
        const std::string* value = find(section, key);
        if (value == nullptr)
        {
            report(section, key, "set");

            return "";
        }

        return value->c_str();
    }

} // namespace cfg
