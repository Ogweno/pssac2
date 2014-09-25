
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

#include <gmt/gmt.h>

#include "sac.h"

#include "gmt_support.h"
#include "array.h"

typedef struct GMT_OPTION GMTOption;
typedef struct polygon polygon;
struct polygon {
  double *x;
  double *y;
  int n;
};

polygon ** polyfillrect(double *x, double *y, int n, int positive, double rect[4]);




enum {
  XMIN = 0,
  XMAX,
  YMIN,
  YMAX,
};

enum {
  SCALE_NORMAL,
  SCALE_ALPHA,
  SCALE_BODY_WAVE,
  SCALE_SURFACE_WAVE,
};

int
error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "pssac2: ");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(-1);
}

int verbose_level = 0;

enum {
  INFO = 1,
  DETAIL,
  DEBUG,
};

void
info(int level, char *fmt, ...) {
  va_list ap;
  if(level > verbose_level) {
    return;
  }
  va_start(ap, fmt);
  fprintf(stderr, "pssac2: ");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

struct Value {
  int active;
  float value;
};
struct Switch {
  int active;
};
struct G {
  int active;
  int r,g,b;
  float zero;
  float time[2];
};
struct Pick {
  int active;
  char pick[2];
  float time[2];
};

typedef struct PSSAC2_CTRL pssac2_ctrl;
struct PSSAC2_CTRL {
  struct Pick C;
  struct E {
    int active;
    char plot_type;
    char alignment;
    float velocity;
  } E;
  struct M {
    int active;
    int scaling;
    double size;
    double alpha;
  } M;
  struct l {
    int active;
    float x,y;
    float len,bar;
    int font_size;
  } l;
  struct G Gp;
  struct G Gn;
  struct Pick S;
  struct Value L;
  struct Switch n;
  struct Switch r;
  struct Switch v;
  struct Switch s;
  struct Switch V;
};


GMTOption *
GMT_Remove_Option(void *API, GMTOption *first, GMTOption *opt) {
  if(opt == first && opt->previous == NULL) {
    first = opt->next;
  }
  GMT_Delete_Option(API, opt);
  return first;
}

int
parse_time_value(char *str, char *pick, float *time) {
  char *p = str;
  *pick = ' ';
  *time = 0.0;
  while(p && *p) {
    switch(*p) {
    case 't':
      info(DEBUG, "time [pick] %s\n", p);
      *pick = 't';
      break;
    case '/':
      return 0;
      break;
    default:
      if(*pick == 't' && ((isdigit(*p) || *p == 'b' || *p == 'o' || *p == 'e' || *p == 'a' || *p == 'z'))) {
        *pick = *p;
        info(DEBUG, "time[pick]: %c\n", *pick);
        return 1;
      } else if(sscanf(p, "%f", time) == 1) {
        info(DEBUG, "time[value]: %lf %s\n", *time, p);
        return 1;
      } else {
        return 0;
      }
      break;
    }
    p++;
  }
  return 0;
}

int
pssac2_parse_value(void *API, GMTOption **options, struct Value *S, char optc) {
  GMTOption *opt;
  S->active = FALSE;
  if((opt = GMT_Find_Option(API, optc, *options)) == NULL) {
    return 0 ;
  }
  S->active = TRUE;
  if(sscanf(opt->arg, "%f", &S->value) != 1) {
    error("Error parsing option -%c%s\n", opt->option, opt->arg);
  }
  *options = GMT_Remove_Option(API, *options, opt);
  return 1;
}

int
pssac2_parse_time_value(void *API, GMTOption **options, struct Pick *S, char optc) {
  GMTOption *opt;
  S->active = FALSE;
  if((opt = GMT_Find_Option(API, optc, *options)) == NULL) {
    return 0 ;
  }
  S->active = TRUE;
  if(!parse_time_value(opt->arg, &S->pick[0], &S->time[0])) {
    error("Error reading in time value: -%c%s\n", opt->option, opt->arg);
  }
  *options = GMT_Remove_Option(API, *options, opt);
  return 1;
}
int
pssac2_parse_switch(void *API, GMTOption **options, struct Switch *n, char optc) {
  GMTOption *opt;
  n->active = FALSE;
  if((opt = GMT_Find_Option(API, optc, *options)) == NULL) {
    return 0;
  }
  n->active = TRUE;
  *options = GMT_Remove_Option(API, *options, opt);
  return 1;
}

void
pssac2_parse_G(void *API, GMTOption **options, struct G *G, char optc) {
  GMTOption *opt;
  G->active = FALSE;
  if((opt = GMT_Find_Option(API, optc, *options)) == NULL) {
    return;
  }
  G->active = TRUE;
  if(sscanf(opt->arg, "%d/%d/%d/%f/%f/%f", &G->r, &G->g, &G->b,
            &G->zero, &G->time[0], &G->time[1]) != 6) {
    error("Error parsing option -%c%s\n", opt->option, opt->arg);
  }
  *options = GMT_Remove_Option(API, *options, opt);
}

void
pssac2_parse_l(void *API, GMTOption **options, struct l *l) {
  GMTOption *opt;
  l->active = FALSE;
  if((opt = GMT_Find_Option(API, 'l', *options)) == NULL) {
    return;
  }
  l->active = TRUE;
  if(sscanf(opt->arg, "%f/%f/%f/%f/%d", &l->x, &l->y,
            &l->len, &l->bar, &l->font_size) != 5) {
    error("Error parsing option -%c%s\n", opt->option, opt->arg);
  }
  *options = GMT_Remove_Option(API, *options, opt);
}

int
gmt_is_xy_projections(void *API, GMTOption *options) {
  int xy;
  GMTOption *J;
  if((J = GMT_Find_Option(API, 'J', options)) == NULL) {
    error("Error, must specify -J option\n");
  }
  info(DEBUG, "J: %s\n", J->arg);
  switch(J->arg[0]) {
  case 'p': case 'P':
  case 'x': case 'X':
    xy = TRUE;
    break;
  default:
    xy = FALSE;
    break;
  }
  return xy;
}

void
pssac2_parse_E(void *API, GMTOption **options, struct E *E) {
  int tn;
  GMTOption *opt;
  if((opt = GMT_Find_Option(API, 'E', *options)) == NULL) {
    error("Error, must specify -E option\n");
  }
  E->active = TRUE;
  E->plot_type = opt->arg[0];
  if(!gmt_is_xy_projections(API, *options)) {
    E->plot_type = 'm';
  }
  E->alignment = opt->arg[1];
  if(E->alignment == 't') {
    if(sscanf(&(opt->arg[2]), "%d", &tn) == 1) {
      switch(tn) {
      case -5: E->alignment = 'b'; break;
      case -3: E->alignment = 'a'; break;
      case -2: E->alignment = 'o'; break;
      case  0: case  1: case  2: case  3: case 4:
      case  5: case  6: case  7: case  8: case 9:
        E->alignment = tn + '0'; break;
      }
    } else if(sscanf(&(opt->arg[2]), "%c", &E->alignment) == 1) {
      info(DETAIL, "Simple alignment\n");
    } else {
      error("Error parsing option '-E%s' time alignment\n", opt->arg);
    }
  } else if(sscanf(&(opt->arg[1]), "%f", &E->velocity) == 1) {
    E->alignment = 'v';
  } else {
    error("Error parsing option '-E%s'\n", opt->arg);
  }
  *options = GMT_Remove_Option(API, *options, opt);
}

void
pssac2_parse_M(void *API, GMTOption **options, struct M *M) {
  GMTOption *opt;
  if((opt = GMT_Find_Option(API, 'M', *options)) == NULL) {
    error("Error, must specify -M option\n");
  }
  M->active = TRUE;
  M->size = 1.0;
  M->alpha = 0.0;
  sscanf(opt->arg, "%lf", &M->size);
  if(strstr(opt->arg, "/s") ) {
    M->scaling = SCALE_SURFACE_WAVE;
    info(DETAIL, "scale: Surface Wave value: %e\n", M->size);
  } else if(strstr(opt->arg, "/b")) {
    M->scaling = SCALE_BODY_WAVE;
    info(DETAIL, "scale: Body Wave value: %e\n", M->size);
  } else if(sscanf(opt->arg, "%lf/%lf", &M->size, &M->alpha) == 2) {
    M->scaling = SCALE_ALPHA;
    info(DETAIL, "scale: Distance value: %e alpha: %e\n", M->size, M->alpha);
  } else {
    GMT_Get_Value(API, opt->arg, &M->size);
    M->scaling = SCALE_NORMAL;
    info(DETAIL, "scale: Normalized: %e\n", M->size);
  }
  *options = GMT_Remove_Option(API, *options, opt);
}

void
pssac2_parse_C(void *API, GMTOption **options, struct Pick *C) {
  int i;
  char *c;
  GMTOption *opt;

  C->active = FALSE;
  if((opt = GMT_Find_Option(API, 'C', *options)) == NULL) {
    return;
  }
  C->active = TRUE;

  info(DETAIL, "CUT: %s\n", opt->arg);
  for(i = 0; i < 2; i++) {
    if(i == 0) {
      c = opt->arg;
    } else {
      c = index(opt->arg, '/');
      if(!(c && *c && *(c+1))) {
        error("Error reading in cut values -C%s\n", opt->arg);
      }
      c++;
    }
    if(! parse_time_value(c, &C->pick[i], &C->time[i])) {
      error("Error reading in cut values -C%s\n", opt->arg);
    }
  }
  *options = GMT_Remove_Option(API, *options, opt);
}

double *
pssac2_plot_scale(void *API, GMTOption *options) {
  double x[4],y[4];
  double *WESN;
  int in_ID, out_ID;
  int status;
  char i_string[16], o_string[16];
  char *buffer;
  struct GMT_VECTOR *V;
  struct GMT_VECTOR *Vo;
  GMTOption *opt, *R, *J;

  WESN = (double *) malloc(sizeof(double) * 8);

  if((opt = GMT_Find_Option(API, 'R', options)) != NULL) {
    GMT_Get_Value(API, opt->arg, WESN);
    x[0] = WESN[XMIN];    y[0] = WESN[YMIN];
    x[1] = WESN[XMIN];    y[1] = WESN[YMAX];
    x[2] = WESN[XMAX];    y[2] = WESN[YMAX];
    x[3] = WESN[XMAX];    y[3] = WESN[YMIN];
    R = opt;
  } else {
    error("Error, must specify -R option\n");
  }
  if((J = GMT_Find_Option(API, 'J', options)) == NULL) {
    error("Error, must specify -J option\n");
  }

  /* Determine plot_scale / cm */
  V   = C_to_GMT_vector(x, y, 4);
  Vo  = GMT_vector(2, 4, GMT_DOUBLE, 0);

  in_ID = GMT_Register_IO(API, GMT_IS_DATASET, 
                          GMT_IS_REFERENCE + GMT_VIA_VECTOR, 
                          GMT_IS_POINT, GMT_IN, NULL, V);
  if(in_ID == GMT_NOTSET) {
    error("Error registering IO/In in plot_scale\n");
  }

  out_ID = GMT_Register_IO(API, GMT_IS_DATASET, 
                           GMT_IS_REFERENCE + GMT_VIA_VECTOR, 
                           GMT_IS_POINT, GMT_OUT, NULL, Vo);
  if(out_ID == GMT_NOTSET) {
    error("Error registering IO/Out in plot_scale\n");
  }
  GMT_Encode_ID(API, i_string, in_ID);
  GMT_Encode_ID(API, o_string, out_ID);


  asprintf(&buffer, "-R%s -J%s -bi -bo -<%s  ->%s ", R->arg, J->arg, i_string, o_string);

  status = GMT_Call_Module(API, "mapproject", GMT_MODULE_CMD, buffer);
  if(status) {
    error("Error calling mapproject in plot_scale\n");
  }

  Vo = GMT_Retrieve_Data(API, out_ID);
  if(Vo == NULL) {
    error("Error getting data from mapproject in plot_scale\n");
  }

  WESN[4 + XMIN] = Vo->data[GMT_X].f8[0];
  WESN[4 + XMAX] = Vo->data[GMT_X].f8[2];
  WESN[4 + YMIN] = Vo->data[GMT_Y].f8[0];
  WESN[4 + YMAX] = Vo->data[GMT_Y].f8[2];
  
  return WESN;
}


float
time_value(SACHEAD *h, char t) {
  float time;
  switch(t) {
  case 'b': time = h->b  ; break;
  case 'o': time = h->o  ; break;
  case 'a': time = h->a  ; break;
  case 'e': time = h->e  ; break;
  case '0': time = h->t0 ; break;
  case '1': time = h->t1 ; break;
  case '2': time = h->t2 ; break;
  case '3': time = h->t3 ; break;
  case '4': time = h->t4 ; break;
  case '5': time = h->t5 ; break;
  case '6': time = h->t6 ; break;
  case '7': time = h->t7 ; break;
  case '8': time = h->t8 ; break;
  case '9': time = h->t9 ; break;
  default:
    error("Unknown time value, '%c' exiting\n", t);
    break;
  }
  if(time == -12345) {
    time = 0.0;
  }
  return time;
}

float 
plot_type_y(SACHEAD *h, struct E E, float y0) {
  info(DETAIL,"Assessing plot type ... [%c]\n", E.plot_type);
  switch(E.plot_type) {
  case 'a': y0 = h->az;     break;
  case 'b': y0 = h->baz;    break;
  case 'n': y0 ++;          break;
  case 'd': y0 = h->gcarc;  break;
  case 'k': y0 = h->dist;   break;
  case 'm': y0 = h->stla;   break;
  }
  return y0;
}

float
plot_alignment(SACHEAD *h, struct E E) {
  float align;
  info(DETAIL, "Aligning data ... [%c]\n", E.alignment);
  if(E.alignment == 'v') {
    align = h->dist / E.velocity;
  } else if(E.alignment == 'z') {
    align = 0.0;
  } else {
    align = time_value(h, E.alignment);
  }
  info(DETAIL, "Aligning data ... [%c] %f\n", E.alignment, align);
  return align;
}

float
plot_amplitude_scale(SACHEAD *h, struct M M, double WESN[4], double WESN_cm[4]) {
  float yscale;
  info(DETAIL, "Scaling data ...[%lf/%lf]\n", M.size, M.alpha);
  yscale = M.size;
  info(DETAIL, "   => %e \n", yscale);
  /* Amplitude Convert: PLOT_UNIT -> PLOT_UNIT with DATA */
  yscale = yscale * (WESN[YMAX] - WESN[YMIN])/(WESN_cm[YMAX] - WESN_cm[YMIN]);
  info(DETAIL, "   => %e\n", yscale);
  info(DETAIL, "   => Plot: %f %f Plot[cm]: %f %f\n", WESN[YMAX], WESN[YMIN], WESN_cm[YMAX], WESN_cm[YMIN]);
  switch(M.scaling) {
  case SCALE_NORMAL:
    info(DEBUG, "   => %e (max: %f min: %f) normal\n", yscale, h->depmax, h->depmin);
    yscale = yscale / (h->depmax - h->depmin);
    break;
  case SCALE_ALPHA:
    info(DEBUG, "   => %e (%f km) alpha: %lf\n", yscale, h->dist, M.alpha);
    yscale *= pow(h->dist, M.alpha);
    break;
  case SCALE_SURFACE_WAVE:
    info(DEBUG, "   => %e (%f degrees) surface wave\n", yscale, h->gcarc);
    yscale *= pow(sin(fabs(h->gcarc))*2.0*M_PI,0.5);
    break;
  case SCALE_BODY_WAVE:
    info(DEBUG, "   => %e  body wave\n", yscale);
    yscale *= 1.0;
    break;
  }
  info(DETAIL, "   => %e\n", yscale);
  return yscale;
}

int
plot_time_range(SACHEAD *h, struct Pick C, float align, float time[2]) {
  int i, n;
  if(C.active) {
    for(i = 0; i < 2; i++) {
      if(C.pick[i] == ' ') {
        time[i] = C.time[i];
      } else {
        time[i] = time_value(h, C.pick[i]) - align;
      }
      info(DEBUG, "time[%d]: %f\n", i, time[i]);
      time[i] = fmax(time[i], h->b - align);
      time[i] = fmin(time[i], h->e - align);
      info(DEBUG, "time[%d]: %f [with %f - %f]\n", i, time[i], h->b-align,h->e-align);
    }
    if(time[1] < time[0]) {
      info(INFO, "Flipping cut times %f %f\n", time[0], time[1]);
      double t = time[0];
      time[0] = time[1];
      time[1] = t;
    }
    if(time[1] - time[0] <= 0) {
      return 0;
    }
    n = lround((time[1] - time[0])/h->delta) ;
  } else {
    time[0] = h->b - align;
    time[1] = h->e - align;
    n = h->npts;
  }
  return n;
}

float
shift_value(SACHEAD *h, struct Pick S) {
  float shift = 0;
  if(S.active) {
    if(S.pick[0] == ' ') {
      shift = S.time[0];
    } else {
      shift = time_value(h, S.pick[0]);
    }
  }
  return shift;
}

struct GMT_DATASEGMENT **
polyfill(struct GMT_DATASEGMENT *seg, struct GMT_DATASEGMENT **pseg, struct G G, int positive, float y0, float yscale, double WESN[4]) {
  int i;
  double rect[4];
  struct GMT_DATASEGMENT *psegg;

  rect[0] = G.time[0];
  rect[1] = G.time[1];

  if(positive) {
    rect[2] = y0 + G.zero * yscale;
    rect[3] = WESN[YMAX];
  } else {
    rect[2] = WESN[YMIN];
    rect[3] = y0 + G.zero * yscale;
  }

  info(DETAIL, "poly: %d %d\n", positive, (int)seg->n_rows);
  polygon **P = polyfillrect(seg->coord[GMT_X], seg->coord[GMT_Y], (int)seg->n_rows, positive, rect);

  for( i = 0; i < xarray_length(P); i++) {
    polygon *p = P[i];
    psegg = gmt_datasegment();
    psegg->n_rows = p->n;
    psegg->coord = (double **) malloc(sizeof(double *) * 2);
    psegg->coord[GMT_X] = p->x;
    psegg->coord[GMT_Y] = p->y;
    asprintf(&psegg->header, " -G%d/%d/%d ", G.r, G.g, G.b);
    pseg = xarray_append(pseg, psegg);
    info(DEBUG, "fill segment: %d/%d n: %d [%p]\n", i, xarray_length(P), p->n, p);
  }
  return pseg;
}

int
main(int argc, char *argv[]) {
  int i,status, in_ID;
  void * API;
  char string[16];
  char *buffer;
  struct GMT_DATASET *data;
  GMTOption *options, *option, *option_tmp;
  struct GMT_DATASEGMENT **pseg;

  double *WESN, *WESN_cm;
  char line[512], *file;
  int use_stdin;

  float *amp;
  SACHEAD h;
  int byte_swap;
  float yscale, y0;//, depmin, depmax;
  float align;
  pssac2_ctrl *Ctrl;

  
  Ctrl = (pssac2_ctrl *) malloc(sizeof(pssac2_ctrl));

  info(DEBUG, "GMT Create Session\n");
  API = GMT_Create_Session("pssac2", 2, 0, NULL);
  if(!API) {
    error("Error creating GMT Session\n");
    exit(-1);
  }
  info(DEBUG, "Create Options\n");
  argv = argv + 1;
  argc = argc - 1;
  options = GMT_Create_Options(API, argc, argv);

  while(pssac2_parse_switch(API, &options, &Ctrl->V, 'V')) {
    verbose_level += 1;
  }

  /* Option Parsing */
  pssac2_parse_C(API, &options, &Ctrl->C);
  pssac2_parse_E(API, &options, &Ctrl->E);
  pssac2_parse_M(API, &options, &Ctrl->M);
  pssac2_parse_l(API, &options, &Ctrl->l);
  pssac2_parse_G(API, &options, &Ctrl->Gp, 'G');
  pssac2_parse_G(API, &options, &Ctrl->Gn, 'g');
  pssac2_parse_time_value(API, &options, &Ctrl->S, 'S');
  pssac2_parse_value(API, &options, &Ctrl->L, 'L');
  pssac2_parse_switch(API, &options, &Ctrl->r, 'r');
  pssac2_parse_switch(API, &options, &Ctrl->v, 'v');

  WESN = pssac2_plot_scale(API, options);
  WESN_cm = &WESN[4];

  /* Send plot data to psxy */

  if(GMT_Init_IO(API, GMT_IS_TEXTSET, GMT_IS_NONE, GMT_IN, GMT_ADD_DEFAULT, 0, NULL) != 0) {
    error("Error calling GMT_Init_IO\n");
  }

  /* Create the Dataset */
  data = gmt_dataset();
  gmt_dataset_append_datatable(data, gmt_datatable());
  
  /* Fill the Dataset */
  y0 = 0.0;
  option = options;
  use_stdin = 0;
  if(!GMT_Find_Option(API, '<', option)) {
    use_stdin = 1;
    option = NULL;
  }
  while((use_stdin && fgets(line, 512, stdin)) ||
        (!use_stdin && (option = GMT_Find_Option(API, '<', option)) != NULL)) {
    if(!use_stdin) {
      file = option->arg;
    } else {
      file = &line[0];
      line[strlen(line)-1] = 0;
    }
    /* Read SAC File */
    info(DETAIL, "sacfile: %s\n", file);
    if((amp = read_sac_swap(file, &h, byte_swap)) == NULL) {
      info(0,"Error reading is %s\n", file);
      goto NEXT;
    }


    struct GMT_DATASEGMENT *seg;
    float time[2];
    int n;

    seg = gmt_datasegment();
    pseg = xarray_new('p');

    /* Y value - Depends on plot type*/
    y0 = plot_type_y(&h, Ctrl->E, y0);

    /* Time Shift to align data */
    align = plot_alignment(&h, Ctrl->E);

    /* Amplitude Scaling */
    yscale = plot_amplitude_scale(&h, Ctrl->M, WESN, WESN_cm);

    /* Time Range */
    if((n = plot_time_range(&h, Ctrl->C, align, time)) <= 0) {
      info(0, "skipping file %s, no data after cut\n", option->arg);
      goto NEXT;
    }

    /* Fill Data Segment */
    int k = 0;
    float ymean, shift, t;
    ymean = (Ctrl->r.active) ? h.depmen : 0.0;
    shift = shift_value(&h, Ctrl->S);

    /* Allocate space for data */
    gmt_datasegment_alloc(seg, 2, n);

    for(i = 0; i < h.npts; i++) {
      t = h.b + i * h.delta - align + shift;
      if(Ctrl->C.active && ( t < time[0] || t > time[1])) {
        info(DEBUG, "%d/%d %f skip %f %f\n", k, (int)seg->n_rows, t, time[0], time[1]);
        continue;
      }
      /* Map Plotting */
      if(Ctrl->E.plot_type == 'm') { 
        t = h.stlo + t / Ctrl->L.value;
      }
      seg->coord[GMT_X][k] = t;
      seg->coord[GMT_Y][k] = y0 + (amp[i]-ymean) * yscale;
      k++;
    }

    /* Create Fill Segments */
    if(Ctrl->Gp.active) {
      pseg = polyfill(seg, pseg, Ctrl->Gp, 1, y0, yscale, WESN);
    }
    if(Ctrl->Gn.active) {
      pseg = polyfill(seg, pseg, Ctrl->Gn, 0, y0, yscale, WESN);
    }

    /* Add time series to dataset */
    gmt_dataset_append_datasegment(data, seg, 0);

  NEXT:
    if(option) {
      option_tmp = option;
      option = option->next;
      options = GMT_Remove_Option(API, options, option_tmp);
    }
  }

  /* Draw Fill segments here */
  for(i = 0; i < xarray_length(pseg); i++) {
    gmt_dataset_append_datasegment(data, pseg[i], 0);
  }

  if(data->n_segments <= 0) {
    error("No data files plotted\n");
  }
  info(INFO, "datafiles plotted: %d\n", data->n_segments);

  if(GMT_Register_IO(API, GMT_IS_DATASET, GMT_IS_REFERENCE, 
                     GMT_IS_LINE, GMT_IN, NULL, data) < 0) {
    error("Error Error registering data to plot\n");
  }

  /* Get and Encode Dataset IDs */
  if((in_ID = GMT_Get_ID(API, GMT_IS_DATASET, GMT_IN, data)) == GMT_NOTSET) {
    error("Error getting data ID\n");
  }

  if(GMT_Encode_ID(API, string, in_ID) != 0) {
    error("Error encoding ID\n");
    exit(-1);
  }

  /* Send Dataset to psxy for plotting */
  buffer = GMT_Create_Cmd(API, options);
  info(DEBUG, "cmd: %s\n", buffer);
  status = GMT_Call_Module(API, "psxy", GMT_MODULE_CMD, buffer);
  if (status) {
    error("Error plotting data\n");
  }
  
  //  if(GMT_Destroy_Session(API)) {
  //    exit(-1);
  //  }
  
  return 0;
}