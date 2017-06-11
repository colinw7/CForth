#include <CForth.h>
#include <CReadLine.h>

void processFile(const std::string &filename);

int
main(int argc, char **argv)
{
  bool debug = false;
  bool init  = true;

  std::vector<std::string> filenames;

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      if      (strcmp(argv[i], "-debug") == 0)
        debug = true;
      else if (strcmp(argv[i], "-no_init") == 0)
        init = false;
      else if (strcmp(argv[i], "-h") == 0 ||
               strcmp(argv[i], "-help") == 0) {
        std::cerr << "CForthTest [-debug] [-noinit] [-h|-help] <filenames>" << std::endl;
        exit(1);
      }
      else
        std::cerr << "Invalid arg: " << argv[i] << std::endl;
    }
    else
      filenames.push_back(argv[i]);
  }

  CForth::setDebug(debug);

  if (init)
    CForth::init();

  if (! filenames.empty()) {
    uint num_files = filenames.size();

    for (uint i = 0; i < num_files; ++i)
      processFile(filenames[i]);
  }
  else {
    CReadLine readline;

    readline.setPrompt("> ");

    std::string line = readline.readLine();

    while (! readline.eof()) {
      if (line == "bye") break;

      if (! CForth::parseLine(line))
        std::cerr << CForth::State::lastError().msg() << std::endl;

      line = readline.readLine();
    }
  }

  return 0;
}

void
processFile(const std::string &filename)
{
  if (! CForth::parseFile(filename.c_str())) {
    std::cerr << CForth::State::lastError().msg() << std::endl;
  }
}
