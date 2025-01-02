#ifndef MAIN_H
#define MAIN_H

#include "../../../lib/LinkMobile.hpp"

#include <string>
#include <vector>

struct DefaultValue {
  std::string name;
  std::string value;
};

void handleP2P();
void handlePPP();
void cleanup();
void readConfiguration();

void printMenu();
void transfer(LinkMobile::DataTransfer& dataTransfer,
              std::string text,
              unsigned char connectionId,
              bool addNullTerminator = false);

std::string getNumberInput();
std::string getPasswordInput();
std::string getDomainInput();
std::string getTextInput(std::string& field,
                         unsigned int maxChars,
                         std::string inputName,
                         std::vector<DefaultValue> defaultValues);
std::string getInput(std::string& field,
                     unsigned int maxChars,
                     std::string inputName,
                     std::vector<std::vector<std::string>> rows,
                     std::vector<std::vector<std::string>> altRows,
                     std::vector<DefaultValue> defaultValues,
                     std::string altName);

std::string getStateString(LinkMobile::State state);
std::string getErrorString(LinkMobile::Error error);
std::string getErrorTypeString(LinkMobile::Error::Type errorType);
std::string getResultString(LinkMobile::CommandResult cmdResult);

void log(std::string text);
std::string toStr(char* chars, int size);
bool didPress(unsigned short key, bool& pressed);
void waitForA();

template <typename I>
[[nodiscard]] std::string toHex(I w, size_t hex_len = sizeof(I) << 1);

#endif  // MAIN_H
