#pragma once

#include <string>
#include <unordered_map>

namespace cfg
{
    //-------------------------------------------------------------------------
    // How a bad read is reported.
    //
    // The default prints the message to stderr and aborts. Install one of your
    // own to route it wherever the program already sends fatal errors; it is
    // handed a message and nothing else, so this library never has to know what
    // kind of program is using it.
    //
    // A handler that returns leaves the getter to hand back a zero of its type.

    using error_handler = void (*)(const char* message);

    void set_error_handler(error_handler handler);

    //-------------------------------------------------------------------------
    // A loaded .ini file: `[section]` headers over `key = value` lines.
    //
    // The format is the small common one: `; comment`, `# comment`, `[section]`,
    // `key = value`. Whitespace around names and values is trimmed, sections and
    // keys are matched case-insensitively, and anything from the first `;` or
    // `#` on a line is a comment - so a value cannot contain either.
    //
    // The whole file is read into memory by load(), so a get is a lookup and
    // never a file access. It is still a lookup that builds and lowercases a
    // string, though: this is built for reading settings once at startup, not
    // for being asked the same question every frame.
    //
    // There is no fallback argument on any getter. A setting the file does not
    // name, or names as something that will not read as the type asked for, is
    // an error - because a fallback is how a mistyped key gets to look like a
    // working one, and how an afternoon goes into wondering why editing the
    // file changes nothing.
    class ini
    {
    public:
        //---------------------------------------------------------------------
        // Loading

        // False when the file cannot be opened, which is the only failure worth
        // reporting back rather than erroring on: whether a missing file is
        // fatal is the caller's decision, not this class's.
        //
        // A malformed line is skipped, so one bad line costs the setting on it
        // and not the whole file. Loading again replaces everything read before.
        bool load(const std::string& path);

        //---------------------------------------------------------------------
        // Reading
        //
        // Const: everything these answer was decided by load(). Each errors
        // through the handler above when the setting is absent, or when its
        // value does not read as this type.

        // For a setting that is genuinely optional. Anything else should just
        // be read, and a missing one left to be the error it is.
        bool has(const std::string& section, const std::string& key) const;

        int get_int(const std::string& section, const std::string& key) const;
        unsigned int get_uint(const std::string& section, const std::string& key) const;
        float get_float(const std::string& section, const std::string& key) const;

        // Accepts true/false, yes/no, on/off and 1/0, in any case.
        bool get_bool(const std::string& section, const std::string& key) const;

        std::string get_string(const std::string& section, const std::string& key) const;

        // The stored text as a pointer into the ini rather than a copy, so a
        // caller that cannot hold a std::string can still read a string
        // setting. Valid until the next load().
        const char* get_cstr(const std::string& section, const std::string& key) const;

    private:
        // Null when the setting is not there, and silent about it - the one
        // lookup every getter shares, before each decides what to say.
        const std::string* find(const std::string& section, const std::string& key) const;

        // Reports through the handler and returns nothing, so a getter can tail
        // call it and then hand back its zero.
        void report(const std::string& section, const std::string& key, const char* expected) const;

        // "section.key", both folded to lower case. One map rather than a map
        // of maps: a section is a prefix, and nothing here ever asks what a
        // section contains.
        std::unordered_map<std::string, std::string> m_settings;

        // Kept so an error can name the file it is about.
        std::string m_path;
    };

} // namespace cfg
