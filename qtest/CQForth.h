#include <QWidget>

class Canvas : public QWidget {
  Q_OBJECT

 public:
  Canvas();

  void paintEvent(QPaintEvent *e);
};
