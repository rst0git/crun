/*
 * crun - OCI runtime written in C
 *
 * Copyright (C) 2020 Adrian Reber <areber@redhat.com>
 * crun is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * crun is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with crun.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>

#include "crun.h"
#include "libcrun/container.h"
#include "libcrun/status.h"
#include "libcrun/utils.h"

enum
{
  OPTION_IMAGE_PATH = 1000,
  OPTION_WORK_PATH,
  OPTION_LEAVE_RUNNING,
  OPTION_TCP_ESTABLISHED,
  OPTION_SHELL_JOB,
  OPTION_EXT_UNIX_SK,
  OPTION_MNG_CG_MODE
};

static char doc[] = "OCI runtime";

static libcrun_checkpoint_restore_t cr_options;

static struct argp_option options[] = {
  {"image-path", OPTION_IMAGE_PATH, "DIR", 0,
   "path for saving criu image files", 0},
  {"work-path", OPTION_WORK_PATH, "DIR", 0,
   "path for saving work files and logs", 0},
  {"leave-running", OPTION_LEAVE_RUNNING, 0, 0,
   "leave the process running after checkpointing", 0},
  {"tcp-established", OPTION_TCP_ESTABLISHED, 0, 0,
   "allow open tcp connections", 0},
  {"ext-unix-sk", OPTION_EXT_UNIX_SK, 0, 0, "allow external unix sockets", 0},
  {"shell-job", OPTION_SHELL_JOB, 0, 0, "allow shell jobs", 0},
  {"manage-cgroups-mode", OPTION_MNG_CG_MODE, "VALUE", 0,
   "cgroups mode: 'soft' (default), 'full' and 'strict'"},
  {0,}
};

static char args_doc[] = "checkpoint CONTAINER";

static error_t
parse_opt (int key, char *arg arg_unused, struct argp_state *state arg_unused)
{
  const char *tmp;

  switch (key)
    {
    case ARGP_KEY_NO_ARGS:
      libcrun_fail_with_error (0, "please specify a ID for the container");

    case OPTION_IMAGE_PATH:
      cr_options.image_path = argp_mandatory_argument (arg, state);
      break;

    case OPTION_WORK_PATH:
      cr_options.work_path = argp_mandatory_argument (arg, state);
      break;

    case OPTION_LEAVE_RUNNING:
      cr_options.leave_running = true;
      break;

    case OPTION_TCP_ESTABLISHED:
      cr_options.tcp_established = true;
      break;

    case OPTION_EXT_UNIX_SK:
      cr_options.ext_unix_sk = true;
      break;

    case OPTION_SHELL_JOB:
      cr_options.shell_job = true;
      break;

    case OPTION_MNG_CG_MODE:
      tmp = argp_mandatory_argument (arg, state);
      if (strcmp (tmp, "full") == 0)
        {
          cr_options.cg_mode = CR_CG_MODE_FULL;
        }
      else if (strcmp (tmp, "strict") == 0)
        {
          cr_options.cg_mode = CR_CG_MODE_STRICT;
        }
      else if (strcmp (tmp, "soft") == 0)
        {
          cr_options.cg_mode = CR_CG_MODE_SOFT;
        }
      else
        {
          libcrun_fail_with_error (0, "unknown cgroup mode specified");
        }

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp run_argp =
  { options, parse_opt, args_doc, doc, NULL, NULL, NULL };

int
crun_command_checkpoint (struct crun_global_arguments *global_args, int argc,
                         char **argv, libcrun_error_t *err)
{
  cleanup_free char *cr_path = NULL;
  int first_arg;
  int ret;

  libcrun_context_t crun_context = { 0, };

  cr_options.cg_mode = CR_CG_MODE_SOFT;

  argp_parse (&run_argp, argc, argv, ARGP_IN_ORDER, &first_arg, &cr_options);
  crun_assert_n_args (argc - first_arg, 1, 2);

  ret =
    init_libcrun_context (&crun_context, argv[first_arg], global_args, err);
  if (UNLIKELY (ret < 0))
    return ret;

  if (cr_options.image_path == NULL)
    {
      cleanup_free char *path = NULL;

      path = get_current_dir_name ();
      if (UNLIKELY (path == NULL))
        libcrun_fail_with_error (0, "realloc failed");

      asprintf (&cr_path, "%s/checkpoint", path);
      if (cr_path == NULL)
        libcrun_fail_with_error (0, "asprintf failed");


      cr_options.image_path = cr_path;
    }

  return libcrun_container_checkpoint (&crun_context, argv[first_arg],
                                       &cr_options, err);
}
