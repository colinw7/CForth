#include <QApplication>
#include <QPainter>

#include <CForth.h>
#include <CQForth.h>

class PlotBuiltin : public CForth::Builtin {
  public:
   PlotBuiltin() :
     CForth::Builtin(USER_BUILTIN, "PLOT") {
   }

   CForth::State exec() override;
};

Canvas *canvas;
QImage  image;

int
main(int argc, char **argv)
{
  QApplication app(argc, argv);

  canvas = new Canvas;

  canvas->resize(256, 256);

  canvas->show();

  bool debug = false;

  std::vector<std::string> filenames;

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      if (strcmp(argv[i], "-debug") == 0)
        debug = true;
    }
    else
      filenames.push_back(argv[i]);
  }

  image = QImage(256, 256, QImage::Format_ARGB32);

  CForth::setDebug(debug);

  CForth::init();

  CForth::defBuiltin<PlotBuiltin>();

  uint num_files = filenames.size();

  for (uint i = 0; i < num_files; ++i) {
    if (! CForth::parseFile(filenames[i].c_str()))
      std::cerr << CForth::State::lastError().msg() << std::endl;
  }

  return app.exec();
}

CForth::State
PlotBuiltin::
exec()
{
  CForth::Number n1, n2, n3;

  if (! CForth::popNumbers(n1, n2, n3))
    return CForth::State::lastError();

  int x = n1.integer();
  int y = n2.integer();
  int c = n3.integer();

  int a = 255;
  int r = std::min(std::max(c, 0), 255);
  int g = 0;
  int b = 0;

  unsigned int color = (((((a << 8) | r) << 8) | g) << 8) | b;

  image.setPixel(x, y, color);

  if (x == 0) {
    canvas->update();

    qApp->processEvents();
  }

  return CForth::State::success();
}

Canvas::
Canvas() :
 QWidget()
{
}

void
Canvas::
paintEvent(QPaintEvent *)
{
  QPainter p(this);

  p.drawImage(0, 0, image);
}
