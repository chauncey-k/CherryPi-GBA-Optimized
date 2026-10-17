from pathlib import Path
import subprocess,tempfile
R=Path(__file__).resolve().parents[1]
source=R/'source/gpsp/cherrypi/settings.c'
harness=r'''
#define ioctl test_ioctl
#define rename test_rename
#include "SETTINGS_SOURCE"
#include <stdarg.h>
static int bad_ioctl,bad_rename,volume_value,switch_value,writes;
int test_ioctl(int fd,unsigned long request,...) {
 va_list ap;void *arg;(void)fd;va_start(ap,request);arg=va_arg(ap,void*);va_end(ap);
 if(bad_ioctl){errno=EIO;return -1;}
 if(request==SNDRV_CTL_IOCTL_ELEM_INFO){struct snd_ctl_elem_info *i=arg;if(!strcmp((char*)i->id.name,"Headphone Playback Volume")){i->type=SNDRV_CTL_ELEM_TYPE_INTEGER;i->count=1;i->value.integer.min=0;i->value.integer.max=63;}else{i->type=SNDRV_CTL_ELEM_TYPE_BOOLEAN;i->count=2;}return 0;}
 if(request==SNDRV_CTL_IOCTL_ELEM_WRITE){struct snd_ctl_elem_value *v=arg;writes++;if(!strcmp((char*)v->id.name,"Headphone Playback Volume"))volume_value=v->value.integer.value[0];else switch_value=v->value.integer.value[0]&&v->value.integer.value[1];return 0;}
 errno=EINVAL;return -1;
}
int test_rename(const char*a,const char*b){if(bad_rename){errno=EROFS;return -1;}return renameat(AT_FDCWD,a,AT_FDCWD,b);}
int main(int argc,char **argv){int mode=atoi(argv[1]);(void)argc;
 if(mode==1){if(!cherrypi_set_brightness(7))return 1;}
 if(mode==2){if(!cherrypi_apply_settings(45,5)||volume_value!=28||!switch_value||writes!=2)return 2;}
 if(mode==3){if(cherrypi_set_brightness(0)||errno!=EINVAL)return 3;}
 if(mode==4){bad_ioctl=1;if(cherrypi_apply_settings(50,5)||!strstr(cherrypi_settings_error(),"ALSA control error"))return 4;}
 if(mode==5){bad_rename=1;if(cherrypi_apply_settings(50,5)||!strstr(cherrypi_settings_error(),"Config save error"))return 5;}
 if(mode==6){FILE *f=fopen(SETTINGS_RUN "/cherrypi-autostart.pid","w");fprintf(f,"%ld\n",(long)getpid());fclose(f);if(!cherrypi_request_poweroff())return 6;}
 if(mode==7){if(!cherrypi_preview_volume(60)||volume_value!=37||!switch_value||writes!=2)return 7;}
 printf("PASS native controls %d\n",mode);return 0;}
'''
with tempfile.TemporaryDirectory(prefix='cherrypi-native-test-') as d:
 p=Path(d);b=p/'backlight';b.mkdir();run=p/'run';run.mkdir();(b/'max_brightness').write_text('10\n');(b/'brightness').write_text('2\n');(p/'control').touch();(run/'cherrypi-autostart.ready').touch()
 config=p/'config';initial='# custom\nVOLUME=65\nROMDIR=/mnt/games/roms\nVOLUME=70\nBRIGHTNESS=2\n'
 (p/'test.c').write_text(harness.replace('SETTINGS_SOURCE',str(source)))
 defs=[f'-DSETTINGS_CONFIG="{config}"',f'-DSETTINGS_BACKLIGHT="{b}"',f'-DSETTINGS_CONTROL="{p/"control"}"',f'-DSETTINGS_RUN="{run}"']
 subprocess.run(['gcc','-std=gnu99','-Wall','-Wextra',*defs,str(p/'test.c'),'-o',str(p/'test')],check=True)
 for case in range(1,8):
  config.write_text(initial);(b/'brightness').write_text('2\n')
  subprocess.run([str(p/'test'),str(case)],check=True)
  if case==1:assert config.read_text()==initial and (b/'brightness').read_text().strip()=='7'
  if case==2:assert config.read_text().count('VOLUME=')==1 and 'VOLUME=45' in config.read_text() and 'BRIGHTNESS=5' in config.read_text() and 'ROMDIR=/mnt/games/roms' in config.read_text()
  if case in (4,5):assert config.read_text()==initial and not list(p.glob('config.new.*'))
  if case==7:assert config.read_text()==initial
  if case==6:assert (run/'cherrypi-poweroff-request').exists()
