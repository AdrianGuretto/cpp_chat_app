// This file is used for defining server structures and errors

#pragma once

#include <string>
#include <unordered_map>

using namespace std::string_literals;

enum class Color{
    Red = 0,
    Green = 1,
    Yellow = 2,
    Pink = 3,
    Cyan = 4,
};

// Make a colored string using ANSI espace sequences
static std::string MakeColorfulText(std::string&& text, Color color){
    const std::unordered_map<Color, std::string> color_to_ansi_seq = {{Color::Red, "\u001b[31m"s}, {Color::Green, "\u001b[32m"s}, {Color::Yellow, "\u001b[33m"s}, {Color::Pink, "\u001b[35m"s}, {Color::Cyan, "\u001b[36m"s}};
    std::string reset_seq = "\u001b[0m"; // to limit text coloring to only our text chunk.
    std::string ret_str;
    ret_str.reserve(text.size() + 2);
    ret_str += color_to_ansi_seq.at(color);
    ret_str.append(std::move(text));
    ret_str += reset_seq;
    return ret_str;
}