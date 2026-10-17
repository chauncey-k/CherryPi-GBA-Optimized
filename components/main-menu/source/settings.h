/* Cherry Pi device control, GPL-2.0-or-later. */
#ifndef CHERRYPI_SETTINGS_H
#define CHERRYPI_SETTINGS_H
int cherrypi_apply_settings(int volume, int brightness);
int cherrypi_preview_volume(int volume);
int cherrypi_set_brightness(int brightness);
int cherrypi_request_poweroff(void);
const char *cherrypi_settings_error(void);
#endif
