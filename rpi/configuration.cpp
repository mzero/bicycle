#include "configuration.h"

#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#include "eventmap.h"


/* Configurations

- A configuration file is a collection of mapping lines
- On each line '#' and following characaters are removed
- Whitespace is ignored between double qutoed tokens

<mapping> ::= <action> ":" <event>?
<action> ::=
 "ignore" | "arm" <layer>? | "clear" | "keep" | "rearm" | "good" | "bad"
  | "mute" <layer> | "volume" <layer>
<layer> ::= '[' <number> ('.' '.' <number>)? ']'

<event> ::= ("ch" <number>)? ("note" | "cc") "" <number> ('.' '.' <number>)?

- For testing purposes, a comment with the word FAIL will invert response:
  if the line fails to parse, it is silently ignored, and if it does parse
  it is reported as an error.
*/

namespace {

  template<class S>
  S& operator<<(S& s, Action a) {
    switch (a) {
      case Action::none:        return s << "none";
      case Action::ignore:      return s << "ignore";

      case Action::arm:         return s << "arm";
      case Action::clear:       return s << "clear";
      case Action::keep:        return s << "keep";
      case Action::rearm:       return s << "rearm";
      case Action::good:        return s << "good";
      case Action::bad:         return s << "bad";

      case Action::layerArm:    return s << "layerArm";
      case Action::layerMute:   return s << "layerMute";
      case Action::layerVolume: return s << "layerVolume";
      default:                  return s << "???";
    }
  }


  EventMap<Command> noteMap(Command(Action::none));
  EventMap<Command> ccMap(Command(Action::none));

  struct ActionClause {
    Action action;
    int layerStart;
    int layerEnd;
  };

  struct EventClause {
    uint8_t status;
    int chStart;
    int chEnd;
    int numStart;
    int numEnd;
  };

  void applyMapping(const ActionClause& ac, const EventClause& ec) {
    // Note: Users spec layers and channel from 1, not 0
    //  note and cc numbers, however, _are_ from 0

    if (ac.layerStart == ac.layerEnd) {
      Command cmd(ac.action, ac.layerStart - 1);
      for (int ch = ec.chStart; ch <= ec.chEnd; ++ch)
        for (int num = ec.numStart; num <= ec.numEnd; ++num) {
          if (ec.status == 0x90)
            noteMap.set(ch - 1, num, cmd);
          else
            ccMap.set(ch - 1, num, cmd);
        }
    } else {
      int nLayers = ac.layerEnd - ac.layerStart + 1;
      int nNums = ec.numEnd - ec.numStart + 1;
      int n = std::min(nLayers, nNums);
      for (int offset = 0; offset < n; ++offset) {
        Command cmd(ac.action, ac.layerStart + offset -1);
        for (int ch = ec.chStart; ch <= ec.chEnd; ++ch) {
          if (ec.status == 0x90)
            noteMap.set(ch - 1, ec.numStart + offset, cmd);
          else
            ccMap.set(ch - 1, ec.numStart + offset, cmd);
        }
      }
    }
  }

  using Error = std::ostringstream;

  std::string errormsg(std::ostream& o) {
    auto e = dynamic_cast<Error*>(&o);
    return e ? e->str() : "parse error";
  }

  class Parse : public std::string {
    public:
      Parse(std::ostream& o) : std::string(errormsg(o)) { }
  };

  ActionClause parseAction(const std::string& actionStr) {
    std::smatch m;

    static const std::regex actionRE(
      "(ignore|arm|clear|keep|rearm|good|bad|mute|volume)"
      "\\s*(?:\\[(\\d+)(?:\\.\\.(\\d+))?\\])?");
    if (!std::regex_match(actionStr, m, actionRE))
      throw Parse(Error() << "malformed action '" << actionStr << "'");

    std::string actionVerb = m.str(1);
    std::string layer1Str = m.str(2);
    std::string layer2Str = m.str(3);

    Action action = Action::none;
    if      (actionVerb == "ignore")    action = Action::ignore;
    else if (actionVerb == "arm")       action = Action::arm;
    else if (actionVerb == "clear")     action = Action::clear;
    else if (actionVerb == "keep")      action = Action::keep;
    else if (actionVerb == "rearm")     action = Action::rearm;
    else if (actionVerb == "good")      action = Action::good;
    else if (actionVerb == "bad")       action = Action::bad;
    else if (actionVerb == "mute")      action = Action::layerMute;
    else if (actionVerb == "volume")    action = Action::layerVolume;
    else
      throw Parse(Error() << "unrecognized action '" << actionVerb);

    if (action == Action::arm && !layer1Str.empty())
      action = Action::layerArm;

    int layer1 = 0;
    int layer2 = 0;
    switch (action) {
      case Action::layerArm:
      case Action::layerMute:
      case Action::layerVolume:
        if (layer1Str.empty())
          throw Parse(Error() << "layer number is missing");

        layer1 = std::stoi(layer1Str);
        if (layer1 < 1 || 100 < layer1)
          throw Parse(Error() << "layer " << layer1 << " out of range");

        layer2 = layer2Str.empty() ? layer1 : std::stoi(layer2Str);
        if (layer2 < layer1 || 100 < layer2)
          throw Parse(Error() << "layer " << layer2 << " out of range");

        break;

      default:
        if (!layer1Str.empty())
          throw Parse(Error() << "unexpected layer number");
    }

    return { action, layer1, layer2 };
  }

  EventClause parseEvent(const std::string& eventStr_) {
    std::string eventStr = eventStr_;
    std::smatch m;

    int chStart = 1;
    int chEnd = 16;

    static const std::regex channelRE("ch\\s*(\\d+)\\s+(.*)");
    if (std::regex_match(eventStr, m, channelRE)) {
      int ch = std::stoi(m.str(1));
      eventStr = m.str(2);

      if (ch < 1 || 16 < ch)
        throw Parse(Error() <<  "channel number " << ch << " out of range");

      chStart = chEnd = ch;
    }

    static const std::regex eventRE("(note|cc)\\s+(\\d+)(?:\\.\\.(\\d+))?");
    if (!std::regex_match(eventStr, m, eventRE))
      throw Parse(Error() << "malformed event '" << eventStr << "'");

    std::string statusStr = m.str(1);
    std::string num1Str = m.str(2);
    std::string num2Str = m.str(3);

    uint8_t status;
    if (statusStr == "note")      status = 0x90;
    else if (statusStr == "cc")   status = 0xb0;
    else
      throw Parse(Error() << "unrecognized event type '" << statusStr);

    int numStart = std::stoi(num1Str);
    if (numStart < 0 || 127 < numStart)
      throw Parse(Error() << numStart << " out of range");

    int numEnd = num2Str.empty() ? numStart : std::stoi(num2Str);
    if (numEnd < numStart || 127 < numEnd)
      throw Parse(Error() << numEnd << " out of range");

    return { status, chStart, chEnd, numStart, numEnd };
  }

  void parseRule(const std::string& line) {
    std::smatch m;

    std::string trimmedLine = line;
    static const std::regex trimWhitespace("\\s*(.*?)\\s*");
    if (std::regex_match(line, m, trimWhitespace))
      trimmedLine = m.str(1);

    if (trimmedLine.empty()) return;

    static const std::regex splitActionAndEvent("([^:]*?)\\s*:\\s*(.*)");
    if (!std::regex_match(trimmedLine, m, splitActionAndEvent))
      throw Parse(Error() << "bad format, missing ':'");

    std::string actionStr = m.str(1);
    std::string eventStr = m.str(2);

    ActionClause ac = parseAction(actionStr);
    if (eventStr.empty())
      return;

    EventClause ec = parseEvent(eventStr);

    applyMapping(ac, ec);
  }

  void parseLine(const std::string& line) {
    std::smatch m;

    bool expect_failure = false;

    std::string uncommentedLine = line;
    static const std::regex decomment("([^#]*)#(.*)");
    if (std::regex_match(line, m, decomment)) {
      uncommentedLine = m.str(1);
      expect_failure = m.str(2).find("FAIL") != std::string::npos;
    }

    try {
      parseRule(uncommentedLine);
    }
    catch (Parse& p) {
      if (expect_failure) return;
      throw;
    }

    if (expect_failure)
      throw Parse(Error() << "was not expected to parse");
  }

  bool parseFile(std::istream& input) {
    int lineNo = 1;
    bool good = true;
    for (std::string line; std::getline(input, line); ++lineNo) {
      try {
        parseLine(line);
      }
      catch (Parse& p) {
        std::cerr << "configuration line " << lineNo << " error: " << p << "\n";
        good = false;
      }
    }
    return good;
  }
}

bool Configuration::begin() {
  bool good = false;
  std::ifstream configFile(Args::configFilePath);
  if (configFile)
    good = parseFile(configFile);
  else
    std::cerr << "Couldn't read configuration file: " <<  Args::configFilePath << "\n";

  if (good && Args::configCheckOnly)
    std::cerr << "Configuration file is valid.\n";
  return good;
}

Command Configuration::command(const MidiEvent& ev) {
  switch (ev.status & 0xf0) {
    case 0x90:    return noteMap.get(ev);
    case 0xb0:    return ccMap.get(ev);
    default:      return Command(Action::none);
  }
}
