/* ischroot: detect if running in a chroot
 *
 * Debian ischroot program
 * Copyright (C) 2011 Aurelien Jarno <aurel32@debian.org>
 *
 * This is free software; see the GNU General Public License version 2
 * or later for copying conditions.  There is NO warranty.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */

void version()
{
  fprintf(stderr, "Debian ischroot, version " PACKAGE_VERSION
          "Copyright (C) 2011 Aurelien Jarno\n"
          "This is free software; see the GNU General Public License version 2\n"
          "or later for copying conditions.  There is NO warranty.\n");
  exit(0);
}

void usage()
{
  fprintf(stderr, "Usage: ischroot [OPTION]\n"
          "  -f, --default-false return false if detection fails\n"
          "  -t, --default-true  return true if detection fails\n"
          "  -V, --version       output version information and exit.\n"
          "  -h, --help          display this help and exit.\n");
  exit(0);
}

/* return 1 if we are operating within a fakechroot environment,
   return 0 otherwise */
int isfakechroot()
{
  const char *fakechroot, *ldpreload;
  return ((fakechroot = getenv("FAKECHROOT")) &&
	  (strcmp("true", fakechroot) == 0) &&
	  (NULL != getenv("FAKECHROOT_BASE")) &&
	  (ldpreload = getenv("LD_PRELOAD")) &&
	  (NULL != strstr(ldpreload, "libfakechroot.so")));
}

#if defined (__linux__)

/* On Linux we can detect chroots by checking if the 
 * devicenumber/inode pair of / are the same as that of 
 * /sbin/init's. This may fail if not running as root or if
 * /proc is not mounted, in which case 2 is returned.
 */

static int ischroot()
{
  struct stat st1, st2;

  if (stat("/", &st1) || stat("/proc/1/root", &st2))
    return 2;
  else if ((st1.st_dev == st2.st_dev) && (st1.st_ino == st2.st_ino))
    return 1;
  else
    return 0;
}

#elif defined (__FreeBSD_kernel__) || defined (__FreeBSD__)

#include <sys/sysctl.h>
#include <sys/user.h>

/* On FreeBSD we can detect chroot by looking for a specific
 * file descriptor pointing to the location of the chroot. There
 * is not need to be root, so it is unlikely to fail in normal 
 * cases, but return 2 if a memory failure or the like happens. */

static int ischroot()
{
  int mib[4];
  size_t kf_len = 0;
  char *kf_buf, *kf_bufp;

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_FILEDESC;
  mib[3] = getpid ();

  if (sysctl (mib, 4, NULL, &kf_len, NULL, 0) != 0)
    return 2;

  kf_buf = kf_bufp = malloc (kf_len);
  if (kf_buf == NULL)
    return 2;

  if (sysctl (mib, 4, kf_buf, &kf_len, NULL, 0) != 0)
  {
    free(kf_buf);
    return 2;
  }

  while (kf_bufp < kf_buf + kf_len)
  {   
    struct kinfo_file *kf = (struct kinfo_file *) (uintptr_t) kf_bufp;

    if (kf->kf_fd == KF_FD_TYPE_JAIL)
    {
      free(kf_buf);
      return 0;
    }
    kf_bufp += kf->kf_structsize;
  }   

  free(kf_buf);
  return 1;
}

#elif defined (__GNU__)

/* On Hurd we can detect chroot by looking at the device number
 * containing /. The device number of the first mounted filesystem
 * equals 3, and due to bug http://savannah.gnu.org/bugs/?23213
 * chroots have to be created on a different filesystem. Return 2
 * if it is not possible to probe this device. */

static int ischroot()
{
  struct stat st;

  if (stat("/", &st))
    return 2;
  else if (st.st_dev == 3)
    return 1;
  else
    return 0;
}

#else

static int ischroot()
{
  return 2;
}

#warning unknown system, chroot detection will always fail

#endif

/* Process options */
int main(int argc, char *argv[])
{
  int default_false = 0;
  int default_true = 0;
  int exit_status;

  for (;;) {
    int c;
    int option_index = 0;

    static struct option long_options[] = {
      {"default-false", 0, 0, 'f'},
      {"default-true", 0, 0, 't'},
      {"help", 0, 0, 'h'},
      {"version", 0, 0, 'V'},
      {0, 0, 0, 0}
    };
    c = getopt_long(argc, argv, "fthV", long_options, &option_index);
    if (c == EOF)
      break;
    switch (c) {
    case 'f':
      default_false = 1;
      break;
    case 't':
      default_true = 1;
      break;
    case 'h':
      usage();
      break;
    case 'V':
      version();
      break;
    default:
      fprintf(stderr, "Try `ischroot --help' for more information.\n");
      exit(1);
    }
  }

  if (default_false && default_true) {
    fprintf(stderr, "Can't default to both true and false!\n");
    fprintf(stderr, "Try `ischroot --help' for more information.\n");
    exit(1);
  }

  if (isfakechroot())
    exit_status = 0;
  else
    exit_status = ischroot();

  if (exit_status == 2) {
    if (default_true)
      exit_status = 0;
    if (default_false)
      exit_status = 1;
  }

  return exit_status;
}
