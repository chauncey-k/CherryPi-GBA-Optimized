/* Native controls avoid spawning a shell from the running emulator.
 * GPL-2.0-or-later. */
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sound/asound.h>
#ifndef SETTINGS_CONFIG
#define SETTINGS_CONFIG "/etc/cherrypi-autostart.conf"
#endif
#ifndef SETTINGS_BACKLIGHT
#define SETTINGS_BACKLIGHT "/sys/class/backlight/backlight"
#endif
#ifndef SETTINGS_CONTROL
#define SETTINGS_CONTROL "/dev/snd/controlC0"
#endif
#ifndef SETTINGS_RUN
#define SETTINGS_RUN "/run"
#endif
static char error_text[96];
static int fail(const char *operation) {
    int saved = errno ? errno : EIO;
    snprintf(error_text, sizeof(error_text), "%s error %d: %.35s", operation, saved, strerror(saved));
    fprintf(stderr, "Device settings: %s\n", error_text);
    errno = saved;
    return 0;
}
const char *cherrypi_settings_error(void) { return error_text; }
static int brightness_limit(void) {
    FILE *fp = fopen(SETTINGS_BACKLIGHT "/max_brightness", "r");
    int limit;
    if (!fp) return -1;
    if (fscanf(fp, "%d", &limit) != 1) { fclose(fp); errno = EINVAL; return -1; }
    if (fclose(fp)) return -1;
    if (limit < 1 || limit > 65535) { errno = EINVAL; return -1; }
    return limit;
}
int cherrypi_set_brightness(int brightness) {
    FILE *fp;
    int limit = brightness_limit(), ok;
    if (limit < 0) return fail("Backlight");
    if (brightness < 1 || brightness > limit) { errno = EINVAL; return fail("Brightness"); }
    fp = fopen(SETTINGS_BACKLIGHT "/brightness", "w");
    if (!fp) return fail("Brightness open");
    ok = fprintf(fp, "%d\n", brightness) > 0;
    if (fclose(fp)) ok = 0;
    return ok ? 1 : fail("Brightness write");
}
int cherrypi_preview_volume(int volume) {
    int fd, ok = 0;
    unsigned i;
    struct snd_ctl_elem_info info;
    struct snd_ctl_elem_value value;
    fd = open(SETTINGS_CONTROL, O_RDWR | O_CLOEXEC);
    if (fd < 0) return fail("ALSA open");
    memset(&info, 0, sizeof(info));
    info.id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
    strcpy((char *)info.id.name, "Headphone Playback Volume");
    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_INFO, &info)) goto done;
    if (info.type != SNDRV_CTL_ELEM_TYPE_INTEGER || info.count < 1 || info.count > 128) { errno = EINVAL; goto done; }
    memset(&value, 0, sizeof(value)); value.id = info.id;
    for (i = 0; i < info.count; ++i)
        value.value.integer.value[i] = info.value.integer.min +
            (info.value.integer.max - info.value.integer.min) * volume / 100;
    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &value)) goto done;
    memset(&info, 0, sizeof(info));
    info.id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
    strcpy((char *)info.id.name, "Headphone Playback Switch");
    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_INFO, &info)) goto done;
    if (info.type != SNDRV_CTL_ELEM_TYPE_BOOLEAN || info.count < 1 || info.count > 128) { errno = EINVAL; goto done; }
    memset(&value, 0, sizeof(value)); value.id = info.id;
    for (i = 0; i < info.count; ++i) value.value.integer.value[i] = 1;
    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &value)) goto done;
    ok = 1;
done:
    if (!ok) { int saved = errno; close(fd); errno = saved; return fail("ALSA control"); }
    close(fd); return 1;
}
static int persist(int volume, int brightness) {
    FILE *in, *out;
    char line[1024], temporary[256];
    int saved;
    snprintf(temporary, sizeof(temporary), "%s.new.%ld", SETTINGS_CONFIG, (long)getpid());
    in = fopen(SETTINGS_CONFIG, "r");
    if (!in) return fail("Config read");
    out = fopen(temporary, "w");
    if (!out) { saved = errno; fclose(in); errno = saved; return fail("Config create"); }
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, "VOLUME=", 7) == 0) continue;
        if (brightness > 0 && strncmp(line, "BRIGHTNESS=", 11) == 0) continue;
        if (fputs(line, out) == EOF) goto failed;
    }
    if (ferror(in)) goto failed;
    if (fprintf(out, "\nVOLUME=%d\n", volume) < 0) goto failed;
    if (brightness > 0 && fprintf(out, "BRIGHTNESS=%d\n", brightness) < 0) goto failed;
    if (fflush(out) || fsync(fileno(out)) || fchmod(fileno(out), 0644)) goto failed;
    fclose(in); in = NULL;
    if (fclose(out)) { out = NULL; goto failed; } out = NULL;
    if (rename(temporary, SETTINGS_CONFIG)) goto failed;
    sync();
    return 1;
failed:
    saved = errno ? errno : EIO;
    if (in) fclose(in);
    if (out) fclose(out);
    unlink(temporary); errno = saved; return fail("Config save");
}
int cherrypi_apply_settings(int volume, int brightness) {
    if (volume < 0 || volume > 100) { errno = EINVAL; return fail("Volume"); }
    if (brightness == 0) {
        if (access(SETTINGS_BACKLIGHT "/max_brightness", F_OK) == 0 || errno != ENOENT) { errno = EINVAL; return fail("Brightness"); }
    } else if (!cherrypi_set_brightness(brightness)) return 0;
    if (!cherrypi_preview_volume(volume)) return 0;
    return persist(volume, brightness);
}
int cherrypi_request_poweroff(void) {
    FILE *fp;
    long pid;
    if (access(SETTINGS_RUN "/cherrypi-autostart.ready", F_OK)) return fail("Supervisor");
    fp = fopen(SETTINGS_RUN "/cherrypi-autostart.pid", "r");
    if (!fp) return fail("Supervisor pid");
    if (fscanf(fp, "%ld", &pid) != 1 || pid <= 1) { fclose(fp); errno = EINVAL; return fail("Supervisor pid"); }
    fclose(fp);
    if (kill((pid_t)pid, 0)) return fail("Supervisor alive");
    fp = fopen(SETTINGS_RUN "/cherrypi-poweroff-request", "w");
    if (!fp) return fail("Shutdown request");
    if (fclose(fp)) return fail("Shutdown request");
    return 1;
}
