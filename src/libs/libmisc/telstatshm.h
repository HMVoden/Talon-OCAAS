/* include file to access the status shared memory */

#ifndef TELSTATSHM_H
#define TELSTATSHM_H

#include "ccdcamera.h"
#include "scan.h"

/* shared memory key; can be anything unlikely ;-)
 * N.B. bug in some ipcrm's prevents removing it if it's greater than 1<<31.
 */
#define	TELSTATSHMKEY	0x4e56361a

/* telescope axes alignment info */
typedef struct {
    int GERMEQ : 1;		/* set if German Eq mount, else 0 */
    int GERMEQ_FLIP : 1;	/* iff GERMEQ: set when flip is in effect */
    int ZENFLIP : 1;		/* set to flip over zenith, else 0 */
    double HT;			/* HA of scope pole, rads */
    double DT;			/* Dec of scope pole, rads */
    double XP;			/* hme to undr clstl pole, rad +ccw frm N */
    double YC;			/* angle from equator to home, rads +N */
    double NP;			/* nonperpendicularity of axes, rads */
    double R0;			/* rotator offset from home to 0 to PA, rads */

    double hneglim, hposlim;	/* iff GERMEQ: copies of minfo[TEL_HM].*lim */
} TelAxes;

/* weather info.
 * N.B. temp and pres are in Now.
 */
#define MAUXTP	3		/* max aux temp probes supported */
typedef struct {
    int wspeed;			/* wind speed, kph */
    int wdir;			/* wind source az, degs E of N */
    char wdirstr[4];		/* "N", "NNE", etc (includes trailing \0) */
    int humidity;		/* % */
    int rain;			/* today's rain total, .1 mm */
    int alert;			/* "or" of applicable WxAlerts bit flags */
    double auxt[MAUXTP];	/* aux temp probe values, C */
    int auxtmask;		/* ((1<<i) & auxtmask) set if auxt[i] is real */
    time_t updtime;		/* set by wxd when these fields are updated */
} WxStats;

/* mask of reasons for current weather alert */
typedef enum {
    WXA_MAXT = 1,		/* temp is above max */
    WXA_MINT = 2,		/* temp is below max */
    WXA_MAXH = 4,		/* humidity is above max */
    WXA_MAXW = 8,		/* wind is above max */
    WXA_RAIN = 16,		/* rain detected */
} WxAlerts;

/* info about each motor.
 * all measures and directions are canonical unless stated as raw.
 * when we say steps we might really mean microsteps.
 */
typedef struct {
    /* config values or state info */
    char axis;			/* pc39 axis code */
    int have : 1;		/* set if we even have this motor */
    int haveenc : 1;		/* set if this axis uses an encoder */
    int enchome : 1;		/* set to use internal enc home, else ext sw */
    int havelim : 1;		/* set if this axis uses limits */
    int posside : 1;		/* set if home is side hit first when going + */
    int homelow : 1;		/* set if home switch is active when low */
    int homing  : 1;		/* set while homing this axis */
    int limiting: 1;		/* set while finding limits on this axis */
    int ishomed : 1;		/* set if motor has been homed */
    int step;			/* raw motor steps per rev */
    int sign;			/* raw motor sign wrt canonical, +/- 1 */
    int estep;			/* raw encoder steps per rev */
    int esign;			/* raw encoder sign wrt to canonical, +/- 1 */
    double limmarg;		/* limit safety margin, rads */
    double maxvel;		/* max abs vel, rads/sec */
    double maxacc;		/* max acc, rads/sec/sec */
    double slimacc;		/* soft limit and severe acc, rads/sec/sec */
    double poslim, neglim;	/* Pos and neg limit switch locations, rads */
    double trencwt;		/* tracking encoder weight: 0-motor .. 1-enc */
    double df;			/* feedback damping factor, 0..1 */
    				/* N.B. focus uses this for steps/micron */

    /* motion info */
    double cvel;		/* commanded velocity, rads/sec */
    double cpos;		/* current position now, rads from home */
    double dpos;		/* desired position now, rads from home */
    int raw;			/* raw count from home (encoder else motor) */

} MotorInfo;

/* a few handy shortcuts to different units */
#define	CVELStp(mp)	((mp)->sign*(int)floor((mp)->step*(mp)->cvel/(2*PI)+.5))
#define	MAXVELStp(mip)	((int)floor((mip)->step*(mip)->maxvel/(2*PI)+.5))
#define	MAXACCStp(mip)	((int)floor((mip)->step*(mip)->maxacc/(2*PI)+.5))
#define	MAXLACCStp(mip)	((int)floor((mip)->step*(mip)->slimacc/(2*PI)+.5))

#define	focscale	df	/* overload df for focus' use */

/* N.B. telescoped relies on first three being for the mount proper */
/* N.B. xobs relies on all in this order for selective home control */
typedef enum {
    TEL_HM,			/* longitudinal motor, "ha" or "az" */
    TEL_DM,			/* latitudinal motor, "dec" or "alt" */
    TEL_RM,			/* field rotator motor */
    TEL_OM,			/* focus motor */
    TEL_IM,			/* filter wheel motor */
    TEL_NM			/* total number of potential motors */
} MotorId;			/* index into minfo[] */

/* telescope states */
typedef enum {
    TS_STOPPED,			/* telescope motionless; no updates occuring */
    TS_HUNTING,			/* searching for tra/dec, then track */
    TS_TRACKING,		/* tracking object at tra/tdec */
    TS_SLEWING,			/* searching for tha/tdec, then stop */
    TS_HOMING,			/* finding home positions */
    TS_LIMITING			/* finding limit positions */
} TelState;

/* dome states */
typedef enum {
    DS_ABSENT,			/* no dome at all */
    DS_STOPPED,			/* dome is motionless */
    DS_ROTATING,		/* dome is rotating */
    DS_HOMING			/* dome is seeking home */
} DomeState;

/* dome shutter states */
typedef enum {
    SH_ABSENT,			/* no shutter at all */
    SH_IDLE,			/* shutter not moving -- position unknown */
    SH_OPENING,			/* shutter is opening */
    SH_CLOSING,			/* shutter is closing */
    SH_OPEN,			/* shutter is open */
    SH_CLOSED			/* shutter is closed */
} DShState;

/* camera states */
typedef enum {
    CAM_IDLE,			/* shutter closed */
    CAM_EXPO,			/* shutter open, expose in progress */
    CAM_READ			/* shutter closed, data being read to host */
} CamState;

/* current state of everything.
 * H refers to the telescope axis of "longitude", be it HA or Az.
 * D refers to the telescope axis of "latitude", be it Dec or Alt.
 */
typedef struct {

    /* time info */
    Now now;			/* current time and location info */
    int dt;			/* update period, ms */

    /* current position now .. what you'd really see centered in camera */
    double CJ2kRA, CJ2kDec;	/* J2000 astrometric RA/Dec, rads */
    double CARA, CAHA, CADec;	/* EOD apparent RA/HA/Dec, rads */
    double Calt, Caz;		/* alt, az, rads */
    double CPA;			/* parallactic angle, rads, + when west */

    /* desired position now .. N.B. iff TEL_HUNTING/SLEWING/TRACKING */
    double DJ2kRA, DJ2kDec;	/* J2000 astrometric RA/Dec, rads */
    double DARA, DAHA, DADec;	/* EOD apparent RA/HA/Dec, rads */
    double Dalt, Daz;		/* alt, az, rads */
    double DPA;			/* parallactic angle, rads, + when west */

    /* position offsets: add to real to form desired */
    double mdha, mddec;		/* mesh corrections, rads */
    double jdha, jddec;		/* jogging offsets, rads, IFF jogging_ison */

    MotorInfo minfo[TEL_NM];	/* motor info */

    /* scope alignment coefficients, all rads */
    TelAxes tax;

    /* various status indicators */
    TelState telstate;		/* telescope state */
    CCDTempStatus coolerstatus;	/* one of CCDTempStatus values */
    CamState camstate;		/* camera state */
    int camtemp;		/* current ccd camera temperature, C */
    int camtarg;		/* target ccd camera temperature */
    char filter;		/* current filter, or < or > if moving */
    int lights;			/* flat lights: -1 none; 0 off; > 0 intensity */
    int autofocus : 1;		/* set when focus is tracking filter and temp */
    int jogging_ison : 1;	/* currently jogged/jogging from target */
    int autodome : 1;		/* set when dome is tracking scope */
    double domeaz;		/* current dome az, rads +E of N */
    double dometaz;		/* dome target azimuth, rads +E of N */
    DomeState domestate;	/* dome state */
    DShState shutterstate;	/* shutter state */

    /* info about the current or next run. filled periodically by telrun */
    Scan scan;

    /* other weather stats */
    WxStats wxs;

} TelStatShm;

/* handy shortcuts that check things for being ready for normal observing */
#define	DOME_READY						\
	    ((telstatshmp->domestate == DS_ABSENT ||		\
		(telstatshmp->autodome &&			\
		 telstatshmp->domestate == DS_STOPPED)) &&	\
	     (telstatshmp->shutterstate == SH_ABSENT ||		\
		(telstatshmp->shutterstate == SH_OPEN)))

#define	FOCUS_READY	(!telstatshmp->minfo[TEL_OM].have	\
				    || telstatshmp->minfo[TEL_OM].cvel == 0)

#define	FILTER_READY	(!telstatshmp->minfo[TEL_IM].have       \
			    || telstatshmp->filter == telstatshmp->scan.filter)

#define	ANY_HOMING	( 					\
			telstatshmp->minfo[TEL_HM].homing ||	\
			telstatshmp->minfo[TEL_DM].homing ||	\
			telstatshmp->minfo[TEL_RM].homing ||	\
			telstatshmp->minfo[TEL_OM].homing ||	\
			telstatshmp->minfo[TEL_IM].homing ||	\
			telstatshmp->domestate == DS_HOMING)

#define	ANY_LIMITING	( 					\
			telstatshmp->minfo[TEL_HM].limiting ||	\
			telstatshmp->minfo[TEL_DM].limiting ||	\
			telstatshmp->minfo[TEL_RM].limiting ||	\
			telstatshmp->minfo[TEL_OM].limiting ||	\
			telstatshmp->minfo[TEL_IM].limiting)

/* handy shortcuts to motor info */
#define HMOT    (&telstatshmp->minfo[TEL_HM])
#define DMOT    (&telstatshmp->minfo[TEL_DM])
#define RMOT    (&telstatshmp->minfo[TEL_RM])
#define OMOT    (&telstatshmp->minfo[TEL_OM])
#define IMOT    (&telstatshmp->minfo[TEL_IM])


/* telaxes.c */
extern void tel_hadec2xy (double H, double D, TelAxes *tap, double *X,
    double *Y);
extern void tel_xy2hadec (double X, double Y, TelAxes *tap, double *H,
    double *D);
extern void tel_hadec2PA (double H, double D, TelAxes *tap, double latitude,
    double *PA);
extern void tel_realxy2ideal (TelAxes *tap, double *Xp, double *Yp);
extern void tel_ideal2realxy (TelAxes *tap, double *Xp, double *Yp);
extern int tel_solve_axes (double H[], double D[], double X[], double Y[],
    int nstars, double ftol, TelAxes *tap, double fitp[]);

#endif // TELSTATSHM_H
