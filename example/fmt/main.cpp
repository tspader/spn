#include <random>
#include <string>
#include <vector>

#include "fmt/color.h"
#include "fmt/format.h"
#include "fmt/ranges.h"

int main() {
  std::vector<std::string> libs = {
    "imgui",
    "sdl3",
    "glfw",
    "lua",
    "ninja",
    "tracy",
    "sqlite"
  };
  std::vector<std::string> programs = { "game engine", "text editor", "compiler", "web server", "operating system", "emulator", "database", "ray tracer", "spreadsheet" };
  std::vector<std::string> adjectives = { "rude", "hoarse", "randy", "sandy", "stinky", "shallow", "hairy", "sweet", "crunk", "bespectacled" };
  std::vector<std::string> verbs = { "slap", "pound", "squelch", "scream", "pump", "guffaw" };
  std::vector<std::string> nouns = { "jazzercise", "gourd", "sandwich", "big toe", "snail", "camper van", "ostrich", "fancy spoon", "comically oversized pair of scissors", "Guernsey cow" };

  std::string madlib = "Dear, diary \n\nToday, I tried a package manager. My friend used it to build a {} using {}, and she said it was really easy. \n\nThe docs said it was {} to install, unlike its competitors, and they were right! The first thing I tried was to once I installed it was to {}, but just printed an error that said my {} was not {}. My {} was not {}? What the hell does that mean?\n\nI looked in the source code to find where the error was generated, but I soon realized the code was full of {}! Really {} {}! Say what you want, but CMake never did THAT. I think this new tool is {}...I'm not sure that I'll keep using it.";

  std::mt19937 rng(std::random_device{}());
  auto pick = [&](const std::vector<std::string>& words) -> const std::string& {
    std::uniform_int_distribution<size_t> index(0, words.size() - 1);
    return words[index(rng)];
  };
  auto blank = [](const std::string& word) {
    return fmt::styled(word, fmt::emphasis::bold | fmt::fg(fmt::color::cyan));
  };

  const std::string& noun = pick(nouns);
  const std::string& adjective = pick(adjectives);
  std::string plural = pick(nouns) + "s";

  fmt::print("Calculating madlib...\n");
  fmt::print(fmt::runtime(madlib),
    blank(pick(programs)), blank(pick(libs)),
    blank(pick(adjectives)), blank(pick(verbs)),
    blank(noun), blank(adjective), blank(noun), blank(adjective),
    blank(plural), blank(pick(adjectives)), blank(plural),
    blank(pick(adjectives)));
  fmt::print("\n");
  return 0;
}
