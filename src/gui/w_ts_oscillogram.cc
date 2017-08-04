#include "w_ts_oscillogram.h"
#include "fl_util.h"
#include "s_math.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <Fl_Knob/Fl_Knob.H>
#include <FL/fl_draw.H>
#include <vector>
#include <cmath>

struct W_TsOscillogram::Impl {
  W_TsOscillogram *Q = nullptr;

  int mx = -1, my = -1;

  float samplerate = 44100;
  float period = 100e-3;
  float scale = 1;
  float yoff = 0;  // -1..1
  float xtrig = 0;  // 0..1
  float ytrig = 0;  // -1..1
  int trigmode = 0;

  // static constexpr float periodmin = 1e-2;
  // static constexpr float periodmax = 1;

  static constexpr float scalemin = 0.4;
  static constexpr float scalemax = 4;

  std::vector<float> timedata;

  Fl_Group *grpctl = nullptr;
  int grpw = 0;
  int grph = 0;

  void create_controls();
  void reposition_controls();
  void changed_tb(float val);
  void changed_sc(float val);
  void changed_yoff(float val);
  void changed_xtrig(float val);
  void changed_ytrig(float val);
  void changed_trigmode(int val);

  class Screen;
  Screen *screen = nullptr;
};

class W_TsOscillogram::Impl::Screen : public Fl_Widget {
 public:
  Screen(Impl *P, int x, int y, int w, int h)
      : Fl_Widget(x, y, w, h), P(P) {}
  void draw() override;
  int handle(int event) override;
 private:
  void draw_back();
  void draw_data();
  void draw_pointer(int x, int y);
  Impl *P = nullptr;
};

W_TsOscillogram::W_TsOscillogram(int x, int y, int w, int h)
    : W_TsVisu(x, y, w, h),
      P(new Impl) {
  P->Q = this;

  int sx = x;
  int sy = y;
  int sw = w;
  int sh = h;

  P->screen = new Impl::Screen(P.get(), sx, sy, sw, sh);
  this->resizable(P->screen);

  P->create_controls();
  P->reposition_controls();

  this->end();
}

W_TsOscillogram::~W_TsOscillogram() {
}

void W_TsOscillogram::update_ts_data(
    float sr, const float *smps, unsigned len) {
  P->samplerate = sr;
  P->timedata.assign(smps, smps + len);
}

void W_TsOscillogram::reset_data() {
  P->timedata.clear();
}

int W_TsOscillogram::Impl::Screen::handle(int event) {
  if (event == FL_ENTER) {
    fl_cursor(FL_CURSOR_CROSS);
    return 1;
  }

  if (event == FL_LEAVE) {
    P->mx = -1;
    P->my = -1;
    fl_cursor(FL_CURSOR_DEFAULT);
    return 1;
  }

  if (event == FL_MOVE) {
    P->mx = Fl::event_x();
    P->my = Fl::event_y();
    return 1;
  }

  return Fl_Widget::handle(event);
}

void W_TsOscillogram::Impl::Screen::draw() {
  int sx = P->screen->x();
  int sy = P->screen->y();
  int sw = P->screen->w();
  int sh = P->screen->h();
  if (sw > 0 && sh > 0) {
    fl_push_clip(sx, sy, sw, sh);
    //
    this->draw_back();
    this->draw_data();
    //
    int mx = P->mx;
    int my = P->my;
    if (mx >= sx && mx < sx+sw && my >= sy && my < sy+sh)
      this->draw_pointer(mx, my);
    fl_pop_clip();
  }
}

void W_TsOscillogram::Impl::Screen::draw_back() {
  int sx = this->x();
  int sy = this->y();
  int sw = this->w();
  int sh = this->h();

  float tb = P->period;
  float sc = P->scale;
  float yoff = P->yoff;
  int trigmode = P->trigmode;

  fl_color(0, 0, 0);
  fl_rectf(sx, sy, sw, sh);

  for (unsigned i = 0; ; ++i) {
    const float vinterval = 0.25;

    float dv = vinterval * i;
    int yv = sy + (sh-1) * (- yoff - dv * sc + 1) / 2;

    if (i == 0) {
      fl_color(100, 100, 100);
      fl_line(sx, yv, sx+sw-1, yv);
    } else {
      float dv2 = -dv;
      int yv2 = sy + (sh-1) * (- yoff - dv2 * sc + 1) / 2;
      if ((std::abs(yv) < 0 || std::abs(yv) >= sh) &&
          (std::abs(yv2) < 0 || std::abs(yv2) >= sh))
        break;
      fl_color(50, 50, 50);
      fl_line(sx, yv, sx+sw-1, yv);
      fl_line(sx, yv2, sx+sw-1, yv2);
    }
  }

  constexpr unsigned xdivs = 20;
  for (unsigned i = 1; i < xdivs; ++i) {
    int xi = sx + sw * i / float(xdivs);
    fl_color(50, 50, 50);
    fl_line(xi, sy, xi, sy+sh-1);
  }

  if (trigmode != 0) {
    fl_color(0, 150, 0);
    float xtrig = P->xtrig;
    int xt = sx + (sw-1) * xtrig;
    fl_line(xt, sy, xt, sy+sh-1);
    float ytrig = P->ytrig;
    int yt = sy + (sh-1) * (-ytrig + 1) / 2;
    fl_line(sx, yt, sx+sw-1, yt);
  }

  fl_font(FL_COURIER, 12);
  fl_color(200, 200, 200);

  char textbuf[64];
  if (tb < 1)
    snprintf(textbuf, sizeof(textbuf), "X = %g ms", tb * 1e3);
  else
    snprintf(textbuf, sizeof(textbuf), "X = %g s", tb);
  textbuf[sizeof(textbuf)-1] = 0;
  fl_draw(textbuf, sx+2, sy+2, 0, 0, FL_ALIGN_LEFT|FL_ALIGN_TOP, nullptr, 0);

  snprintf(textbuf, sizeof(textbuf), "Y = %g V", 2 / sc);
  textbuf[sizeof(textbuf)-1] = 0;
  fl_draw(textbuf, sx+2, sy+16, 0, 0, FL_ALIGN_LEFT|FL_ALIGN_TOP, nullptr, 0);
}

void W_TsOscillogram::Impl::Screen::draw_data() {
  int sx = this->x();
  int sy = this->y();
  int sw = this->w();
  int sh = this->h();

  const float *p = P->timedata.data();
  unsigned n = P->timedata.size();

  if (n == 0)
    return;

  auto safe_data = [p, n](int idx) {
    return idx < 0 || (unsigned(idx) >= n) ? 0 : p[idx]; };

  fl_color(255, 0, 0);

  float sr = P->samplerate;
  float tb = P->period;
  float sc = P->scale;
  float yoff = P->yoff;
  float xtrig = P->xtrig;
  float ytrig = P->ytrig;

  int trigmode = P->trigmode;
  bool trigup = trigmode & 1;
  bool trigdown = trigmode & 2;

  int xi {};
  int yi {};
  float idf0 = n - 1 - tb * sr;

  bool trigd = false;
  if (trigmode != 0) {
    float ylevel = (ytrig - yoff) / sc;
    float xdelta = xtrig * tb * sr;
    int idt = idf0 + xdelta;
    float y1 = safe_data(idt + 1);
    float y2 {};
    for (; idt >= 0 && !trigd; --idt) {
      y2 = y1;
      y1 = safe_data(idt);
      if ((trigup && y1 <= ylevel && y2 >= ylevel) ||
          (trigdown && y1 >= ylevel && y2 <= ylevel)) {
        float frac = (ylevel - y1) / (y2 - y1);
        idf0 = idt - xdelta + frac;
        trigd = true;
      }
    }
  }

  for (int i = 0; i < sw; ++i) {
    float r = i / float(sw-1);
    float idf = idf0 + r * tb * sr;

    int idx = idf;
    float mu = idf - idx;

    constexpr int nsmp = 4;
    float ismp[nsmp];
    for (int i = 0; i < nsmp; ++i)
      ismp[i] = safe_data(idx + i);

    // float smp = interp_linear(ismp, mu);
    float smp = interp_catmull(ismp, mu);

    int oldxi = xi;
    int oldyi = yi;
    xi = sx + i;
    yi = sy + (sh-1) * (- yoff - smp * sc + 1) / 2;

    if (i > 0)
      fl_line(oldxi, oldyi, xi, yi);
  }

  if (trigmode != 0) {
    const char textbuf[] = "Trig";
    fl_font(FL_COURIER, 12);
    int tw = 8+std::ceil(fl_width(textbuf));
    int th = 16;
    int tx = sx+sw-1-tw-150;
    int ty = sy+4;
    int tal = FL_ALIGN_LEFT|FL_ALIGN_CENTER;
    fl_color(50, 50, 50);
    fl_rectf(tx, ty, tw, th);
    fl_color(200, 200, 200);
    fl_rect(tx, ty, tw, th);
    if (trigd)
      fl_color(0, 250, 0);
    else
      fl_color(0, 150, 0);
    fl_draw(textbuf, tx + 4, ty, tw, th, tal, nullptr, 0);
  }
}

void W_TsOscillogram::Impl::Screen::draw_pointer(int mx, int my) {
  int sx = this->x();
  int sy = this->y();
  int sw = this->w();
  int sh = this->h();

  float tb = P->period;
  float sc = P->scale;
  float yoff = P->yoff;

  float t = tb * (mx-sx) / (sw-1);
  float v = (1 - yoff - 2*float(my-sy)/(sh-1)) / sc;

  fl_color(150, 150, 150);
  fl_line(mx, sy, mx, sy+sh-1);
  fl_line(sx, my, sx+sw-1, my);

  char textbuf[64];
  if (t < 1)
    snprintf(textbuf, sizeof(textbuf), "%.2f ms %.2f V", t * 1e3f, v);
  else
    snprintf(textbuf, sizeof(textbuf), "%.2f s %.2f V", t, v);
  textbuf[sizeof(textbuf)-1] = 0;

  fl_font(FL_COURIER, 12);
  int tw = 8+std::ceil(fl_width("000.00 ms -0.00 V"));
  int th = 16;
  int tx = sx+sw-1-tw-4;
  int ty = sy+4;
  int tal = FL_ALIGN_LEFT|FL_ALIGN_CENTER;
  fl_color(50, 50, 50);
  fl_rectf(tx, ty, tw, th);
  fl_color(200, 200, 200);
  fl_rect(tx, ty, tw, th);
  fl_draw(textbuf, tx+4, ty, tw, th, tal, nullptr, 0);
}

void W_TsOscillogram::resize(int x, int y, int w, int h) {
  W_TsVisu::resize(x, y, w, h);
  P->reposition_controls();
}

void W_TsOscillogram::Impl::create_controls() {
  int mh = 60;

  Fl_Group *grpctl = this->grpctl = new Fl_Group(0, 0, 1, mh);
  grpctl->begin();
  grpctl->resizable(nullptr);

  int interx = 8;
  int curx = 0;

  Fl_Group *box {};
  Fl_Knob *knob {};
  Fl_Choice *choice {};

  int knobw = 42;
  int knobh = 42;

  box = new Fl_Group(0, 0, 1, mh);
  box->begin();
  box->resizable(nullptr);
  box->color(fl_rgb_color(191, 218, 255));
  box->box(FL_ENGRAVED_BOX);
  curx += interx;

  knob = new Fl_Knob(curx, 15, knobw, knobh, "X. period");
  knob->type(Fl_Knob::LINELOG_3);
  knob->labelsize(10);
  knob->align(FL_ALIGN_TOP);
  // knob->value(this->period / this->periodmax);
  knob->value((std::log10(this->period) + 2) / 2);
  VALUE_CALLBACK(knob, this, changed_tb);
  curx += knob->w() + interx;

  knob = new Fl_Knob(curx, 15, knobw, knobh, "Y. scale");
  knob->type(Fl_Knob::LINELIN);
  knob->labelsize(10);
  knob->align(FL_ALIGN_TOP);
  knob->value(this->scale / this->scalemax);
  VALUE_CALLBACK(knob, this, changed_sc);
  curx += knob->w() + interx;

  knob = new Fl_Knob(curx, 15, knobw, knobh, "Y. offset");
  knob->type(Fl_Knob::LINELIN);
  knob->labelsize(10);
  knob->align(FL_ALIGN_TOP);
  knob->range(-1, +1);
  knob->value(this->yoff);
  VALUE_CALLBACK(knob, this, changed_yoff);
  curx += knob->w() + interx;

  box->size(curx - box->x(), mh);
  box->end();

  box = new Fl_Group(curx, 0, 1, mh);
  box->begin();
  box->resizable(nullptr);
  box->color(fl_rgb_color(226, 189, 255));
  box->box(FL_ENGRAVED_BOX);
  curx += interx;

  knob = new Fl_Knob(curx, 15, knobw, knobh, "X. trig");
  knob->type(Fl_Knob::LINELIN);
  knob->labelsize(10);
  knob->align(FL_ALIGN_TOP);
  knob->value(this->xtrig);
  VALUE_CALLBACK(knob, this, changed_xtrig);
  curx += knob->w() + interx;

  knob = new Fl_Knob(curx, 15, knobw, knobh, "Y. trig");
  knob->type(Fl_Knob::LINELIN);
  knob->labelsize(10);
  knob->align(FL_ALIGN_TOP);
  knob->range(-1, +1);
  knob->value(this->ytrig);
  VALUE_CALLBACK(knob, this, changed_ytrig);
  curx += knob->w() + interx;

  choice = new Fl_Choice(curx, 15, 65, knobh, "Trig. mode");
  choice->labelsize(10);
  choice->align(FL_ALIGN_TOP);
  choice->textfont(FL_COURIER);
  choice->textsize(12);
  choice->add("None");
  choice->add("Up");
  choice->add("Down");
  choice->add("Both");
  choice->value(this->trigmode);
  VALUE_CALLBACK(choice, this, changed_trigmode);
  curx += choice->w() + interx;
#warning TODO

  box->size(curx - box->x(), mh);
  box->end();

  int grpw = this->grpw = curx;
  int grph = this->grph = mh;
  grpctl->size(grpw, grph);

  grpctl->end();
}

void W_TsOscillogram::Impl::reposition_controls() {
  int x = Q->x();
  int y = Q->y();
  int h = Q->h();
  Fl_Group *grpctl = this->grpctl;
  grpctl->resize(x, y+h-this->grph, this->grpw, this->grph);
}

void W_TsOscillogram::Impl::changed_tb(float val) {
  // float tb = clamp(val * this->periodmax, this->periodmin, this->periodmax);
  float tb = std::pow(10.0f, -2 + 2 * val);
  this->period = tb;
  Q->redraw();
}

void W_TsOscillogram::Impl::changed_sc(float val) {
  float sc = clamp(val * this->scalemax, this->scalemin, this->scalemax);
  this->scale = sc;
  Q->redraw();
}

void W_TsOscillogram::Impl::changed_yoff(float val) {
  this->yoff = val;
  Q->redraw();
}

void W_TsOscillogram::Impl::changed_xtrig(float val) {
  this->xtrig = val;
  Q->redraw();
}

void W_TsOscillogram::Impl::changed_ytrig(float val) {
  this->ytrig = val;
  Q->redraw();
}

void W_TsOscillogram::Impl::changed_trigmode(int val) {
  this->trigmode = val;
  Q->redraw();
}
