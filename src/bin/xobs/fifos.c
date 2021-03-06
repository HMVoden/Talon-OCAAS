/* handle the fifo work */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "cliserv.h"
#include "configfile.h"
#include "db.h"
#include "misc.h"
#include "strops.h"
#include "telenv.h"
#include "telstatshm.h"
#include "xtools.h"

#include "widgets.h"
#include "xobs.h"


static void tel_rd_cb(XtPointer client, int *fdp, XtInputId *idp);
static void filter_rd_cb(XtPointer client, int *fdp, XtInputId *idp);
static void focus_rd_cb(XtPointer client, int *fdp, XtInputId *idp);
static void dome_rd_cb(XtPointer client, int *fdp, XtInputId *idp);
static void lights_rd_cb(XtPointer client, int *fdp, XtInputId *idp);
static void cam_rd_cb(XtPointer client, int *fdp, XtInputId *idp);

/* this is used to describe the several FIFOs used to communicate with
 * the telescoped.
 */
typedef struct {
  char *name;             /* fifo name */
  FifoId fid;             /* cross-check */
  XtInputCallbackProc cb; /* callback to process input from this fifo */
  int fd[2];              /* file descriptor to/from the daemon, once opened */
  int fdopen;             /* set when fd[] is in use */
  XtInputId id;           /* X connection to fifo */
} FifoInfo;

/* list of fifos to the control daemons */
static FifoInfo fifos[] = {
    {"Tel",	Tel_Id,		tel_rd_cb},
    {"Filter",	Filter_Id,	filter_rd_cb},
    {"Focus",	Focus_Id,	focus_rd_cb},
    {"Dome",	Dome_Id,	dome_rd_cb},
    {"Lights",	Lights_Id,	lights_rd_cb},
    {"Camera",	Cam_Id,		cam_rd_cb},
};

#define NFIFOS XtNumber(fifos)

/* write a message to given fifo -- generally die if real trouble but we
 * do return -1 if fifo is not available now.
 */
int fifoMsg(FifoId fid, char *fmt, ...) {
  FifoInfo *fip = &fifos[fid];
  char buf[512];
  va_list ap;

  /* cross-check */
  if (fip->fid != fid) {
    printf("Bug! fifoMsg fifo cross-check failed:%d %d\n", fip->fid, fid);
    die();
  }

  /* fifos can be closed while telrun is on; passive; or if no camera */
  if (!fip->fdopen) {
    msg("Service not available.");
    return (-1);
  }

  /* format into buf */
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);

  /* clear out any stale message */
  msg(" ");

  /* send command */
  if (cli_write(fip->fd, buf, buf) < 0) {
    printf("%s: %s\n", fip->name, buf);
    die();
  }

  /* ok */
  return (0);
}

/* read a message from the given fifo.
 * if ok, return the leading code value and put the remainder in buf[],
 * else print and die.
 */
int fifoRead(FifoId fid, char buf[], int buflen) {
  FifoInfo *fip = &fifos[fid];
  int v;

  /* cross-check */
  if (fip->fid != fid) {
    printf("Bug! fifoRd fifo cross-check failed: %d %d\n", fip->fid, fid);
    die();
  }

  if (cli_read(fip->fd, &v, buf, buflen) < 0) {
    printf("%s: %s\n", fip->name, buf);
    die();
  }
  return (v);
}

/* send all the daemons a reset command.
 * also reload our own info.
 * N.B. we assume there can not be any aux functions running.
 */

void resetSW() {
  /* read ours */
  initCfg();

  /* always send to all in case being turned off/on */
  fifoMsg(Tel_Id, "Reset");
  fifoMsg(Dome_Id, "Reset");
  fifoMsg(Filter_Id, "Reset");
  fifoMsg(Focus_Id, "Reset");
  fifoMsg(Lights_Id, "Reset");

  /* keep up with filter assignments */
  fillFilterMenu();
  afoc_initCfg();
}

/* shut down all activity */
void stop_all_devices() {
  fifoMsg(Tel_Id, "Stop");
  if (telstatshmp->domestate != DS_ABSENT ||
      telstatshmp->shutterstate != SH_ABSENT)
    fifoMsg(Dome_Id, "Stop");
  if (IMOT->have)
    fifoMsg(Filter_Id, "Stop");
  if (OMOT->have)
    fifoMsg(Focus_Id, "Stop");
}

/* make connections to daemons.
 * N.B. do nothing gracefully if connections are already ok.
 * N.B. not fatal if no camerad.
 */
void initPipes() {
  FifoInfo *fip;

  for (fip = fifos; fip < &fifos[NFIFOS]; fip++) {
    char buf[1024];

    if (!fip->fdopen) {
      if (cli_conn(fip->name, fip->fd, buf) == 0) {
        fip->fdopen = 1;
        fprintf(stderr, "%s opened\n", fip->name);
      } else if (fip->fid != Cam_Id) {
        fprintf(stderr, "%s: %s\n", fip->name, buf);
        die();
      }
    }

    if (fip->fdopen && fip->id == 0)
      fip->id = XtAppAddInput(app, fip->fd[0], (XtPointer)XtInputReadMask,
                              fip->cb, 0);
  }
}

/* close connections to daemons.
 * N.B. do nothing gracefully if connections are already closed down.
 */
void closePipes() {
  FifoInfo *fip;

  for (fip = fifos; fip < &fifos[NFIFOS]; fip++) {
    if (fip->id) {
      XtRemoveInput(fip->id);
      fip->id = 0;
    }
    if (fip->fdopen) {
      (void)close(fip->fd[0]);
      (void)close(fip->fd[1]);
      fip->fdopen = 0;
    }
  }
}

/* called whenever we get input from the Tel fifo */
/* ARGSUSED */
static void tel_rd_cb(client, fdp, idp) 
XtPointer client; /* unused */
int *fdp;       /* pointer to file descriptor */
XtInputId *idp; /* pointer to input id */
{
  char buf[1024];

  fifoRead(Tel_Id, buf, sizeof(buf));
  msg("Telescope: %s", buf);
  updateStatus(1);
}

/* called whenever we get input from the Filter fifo */
/* ARGSUSED */
static void filter_rd_cb(client, fdp, idp) 
XtPointer client; /* unused */
int *fdp;       /* pointer to file descriptor */
XtInputId *idp; /* pointer to input id */
{
  char buf[1024];

  fifoRead(Filter_Id, buf, sizeof(buf));
  msg("Filter: %s", buf);
  updateStatus(1);
}

/* called whenever we get input from the Focus fifo */
/* ARGSUSED */
static void
focus_rd_cb (client, fdp, idp)
XtPointer client;       /* unused name */
int *fdp;               /* pointer to file descriptor */
XtInputId *idp;         /* pointer to input id */
{
	char buf[1024];
	int s;

	s = fifoRead (Focus_Id, buf, sizeof(buf));
	msg ("Focus: %s", buf);
	afoc_foc_cb(s, buf);	/* tell autofocus in case he's running */
	updateStatus(1);
}

/* called whenever we get input from the Dome fifo */
/* ARGSUSED */
static void dome_rd_cb(client, fdp, idp)
XtPointer client; /* file name */
int *fdp;       /* pointer to file descriptor */
XtInputId *idp; /* pointer to input id */
{
  char buf[1024];

  fifoRead(Dome_Id, buf, sizeof(buf));
  msg("Dome: %s", buf);

  updateStatus(1);
}

/* called whenever we get input from the Lights fifo */
/* ARGSUSED */
static void lights_rd_cb(client, fdp, idp) 
XtPointer client; /* file name */
int *fdp;       /* pointer to file descriptor */
XtInputId *idp; /* pointer to input id */
{
  char buf[1024];

  fifoRead(Lights_Id, buf, sizeof(buf));
  msg("Lights: %s", buf);

  updateStatus(1);
}

/* called whenever we get input from the camerad fifo */
/* ARGSUSED */
static void cam_rd_cb(client, fdp, idp) 
XtPointer client; /* unused name */
int *fdp;       /* pointer to file descriptor */
XtInputId *idp; /* pointer to input id */
{
  char buf[1024];
  int s;

  s = fifoRead(Cam_Id, buf, sizeof(buf));
  msg("Camera: %s", buf);
  afoc_cam_cb(s, buf);
}
