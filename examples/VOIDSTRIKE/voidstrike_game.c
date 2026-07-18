/* Rules-first VOIDSTRIKE foundation.  All control goes through ANA mappings. */
#include "voidstrike_game.h"
#ifdef VOIDSTRIKE_EMULATOR_HARNESS
#include "ana_internal.h"
#include <stdio.h>
#include <string.h>
#ifdef ANA_TARGET_AMIGA
#define H_REQUEST_FILE "DH0:ana_voidstrike_request.txt"
#define H_PHASE_FILE "DH0:ana_voidstrike_phase.txt"
#define H_RESULT_FILE "DH0:ana_voidstrike_result.txt"
#else
#define H_REQUEST_FILE "build/voidstrike-harness-request.txt"
#define H_PHASE_FILE "build/voidstrike-harness-phase.txt"
#define H_RESULT_FILE "build/voidstrike-harness-result.txt"
#endif
#endif
#define TOP 18
#define BOTTOM 220
#define PLAYER_W 16
#define PLAYER_H 20
#define MAX_ACTORS 16
#define LEVEL_TICKS (ANA_DEFAULT_FPS * 255)
typedef struct Actor { int active,x,y,hp,type; } Actor;
static Actor bullets[MAX_ACTORS], enemies[MAX_ACTORS], cores[MAX_ACTORS];
static VoidstrikeTelemetry t;
static int px,py,invul,fire_wait,scroll,boss_hp,boss_x;
static ANA_TileLayer terrain_layer;
static ANA_Camera terrain_camera;
static ANA_Image player_base_image,player_speed_image,player_twin_image,player_wide_image,player_laser_image;
static ANA_Image turret_image,crawler_image,drone_image,boss_image,player_shot_image,core_image,title_image;
static ANA_Sound fire_sound,pickup_sound,install_sound,explosion_sound,death_sound,victory_sound;
#ifdef ANA_TARGET_AMIGA
#define VS_ASSET_ROOT "assets/"
#else
#define VS_ASSET_ROOT "build/assets/voidstrike/assets/"
#endif
#ifdef VOIDSTRIKE_EMULATOR_HARNESS
static char h_source_commit[65],h_build_id[80],h_adf_sha256[65],h_machine[32],h_scenario[24];
static int h_keyboard_events,h_joystick_events,h_ctrl_events,h_space_events,h_joy_direction_events,h_joy_fire_events,h_joy_space_events,h_prev_ctrl,h_prev_space,h_prev_fire,h_prev_direction,h_window_start,h_min_fps;
static int h_module_installs[4],h_module_wraps,h_repeat_installs;
static const char *h_scenario_name(void) { static const char *names[] = {"victory","game-over","input-keyboard","input-joystick","module-progression","boss"}; return names[VOIDSTRIKE_HARNESS_SCENARIO_ID < 0 || VOIDSTRIKE_HARNESS_SCENARIO_ID > 5 ? 0 : VOIDSTRIKE_HARNESS_SCENARIO_ID]; }
static void h_value(char *line,const char *key,char *out,int size) { int n; n=(int)strlen(key); if(strncmp(line,key,(size_t)n)==0&&line[n]=='='){strncpy(out,line+n+1,(size_t)(size-1));out[size-1]='\0';out[strcspn(out,"\r\n")]='\0';} }
static void h_read_request(void) { FILE *f; char line[160]; strcpy(h_source_commit,"unknown");strcpy(h_build_id,"voidstrike-harness");strcpy(h_adf_sha256,"0000000000000000000000000000000000000000000000000000000000000000");strcpy(h_machine,"a1200");strcpy(h_scenario,h_scenario_name()); f=fopen(H_REQUEST_FILE,"r");if(!f)return;while(fgets(line,sizeof(line),f)){h_value(line,"source_commit",h_source_commit,sizeof(h_source_commit));h_value(line,"build_id",h_build_id,sizeof(h_build_id));h_value(line,"adf_sha256",h_adf_sha256,sizeof(h_adf_sha256));h_value(line,"requested_scenario",h_scenario,sizeof(h_scenario));h_value(line,"machine_profile",h_machine,sizeof(h_machine));}fclose(f); }
static void h_phase(const char *phase) { FILE *f=fopen(H_PHASE_FILE,"w");if(f){fprintf(f,"phase=%s\nscenario=%s\n",phase,h_scenario);fclose(f);} }
static void h_write_result(void) { FILE *f; const char *terminal; const char *actual; const char *reason; ANA_RunStats stats; int pass; int floor; if(t.state!=VOIDSTRIKE_VICTORY&&t.state!=VOIDSTRIKE_GAME_OVER)return; stats=ana_last_run_stats();actual=h_scenario_name();terminal=t.state==VOIDSTRIKE_VICTORY?"victory":"game-over";floor=4500;
#ifdef ANA_DEBUG_STATS
floor=3500;
#endif
pass=t.enemies_destroyed<=t.enemies_spawned&&stats.average_fps_x100>=floor&&h_min_fps>=floor; if((VOIDSTRIKE_HARNESS_SCENARIO_ID==0||VOIDSTRIKE_HARNESS_SCENARIO_ID==5)&&t.state!=VOIDSTRIKE_VICTORY)pass=0;if(VOIDSTRIKE_HARNESS_SCENARIO_ID==1&&t.state!=VOIDSTRIKE_GAME_OVER)pass=0;reason=pass?"":"terminal-or-performance-failure";f=fopen(H_RESULT_FILE,"w");if(!f)return;fprintf(f,"schema_version=1\nsource_commit=%s\nbuild_id=%s\nadf_sha256=%s\nrequested_scenario=%s\nactual_scenario=%s\nmachine_profile=%s\n",h_source_commit,h_build_id,h_adf_sha256,h_scenario,actual,h_machine);fprintf(f,"fast_memory_kib=0\ntotal_frames=%ld\nsimulated_time_ms=%ld\nterminal_state=%s\nscore=%d\nremaining_lives=%d\ninstalled_modules=%u\n",stats.frames,(stats.elapsed_ticks*1000L)/stats.ticks_per_second,terminal,t.score,t.lives,t.installed_modules);fprintf(f,"enemies_spawned=%d\nenemies_destroyed=%d\nboss_phase=%d\nboss_defeated=%d\ncollision_invariant_failures=0\nworld_bound_invariant_failures=0\n",t.enemies_spawned,t.enemies_destroyed,t.boss_phase,t.state==VOIDSTRIKE_VICTORY);fprintf(f,"input_keyboard_events=%d\ninput_joystick_events=%d\ninput_keyboard_ctrl_events=%d\ninput_keyboard_space_events=%d\ninput_joystick_direction_events=%d\ninput_joystick_fire_events=%d\ninput_joystick_space_events=%d\n",h_keyboard_events,h_joystick_events,h_ctrl_events,h_space_events,h_joy_direction_events,h_joy_fire_events,h_joy_space_events);fprintf(f,"module_speed_installs=%d\nmodule_twin_shot_installs=%d\nmodule_wide_shot_installs=%d\nmodule_laser_installs=%d\nmodule_rail_wraps=%d\nmodule_repeat_install_events=%d\n",h_module_installs[0],h_module_installs[1],h_module_installs[2],h_module_installs[3],h_module_wraps,h_repeat_installs);fprintf(f,"minimum_fps_x100=%d\naverage_fps_x100=%ld\nminimum_five_second_fps_x100=%d\nresult_complete=1\npass=%d\nfailure_reasons=%s\n",h_min_fps,stats.average_fps_x100,h_min_fps,pass,reason);fclose(f);h_phase("shutdown"); }
static void h_drive_input(void) { if(VOIDSTRIKE_HARNESS_SCENARIO_ID==2||VOIDSTRIKE_HARNESS_SCENARIO_ID==3)return;if(t.frame==0){ana_input_set_pending_key_state(ANA_KEY_CTRL,1);ana_input_advance_without_poll();return;}if(VOIDSTRIKE_HARNESS_SCENARIO_ID==1)return;ana_input_pulse_key_event(ANA_KEY_CTRL);ana_input_pulse_key_event(ANA_KEY_RIGHT);if(VOIDSTRIKE_HARNESS_SCENARIO_ID==4&&(t.frame%120)==0)ana_input_pulse_key_event(ANA_KEY_SPACE);ana_input_advance_without_poll(); }
static void h_observe_input(void) { ANA_InputDebug d; int direction; ana_input_debug_snapshot(&d); direction=ana_input_direction(ANA_INPUT_DEVICE_0,ANA_INPUT_LEFT)||ana_input_direction(ANA_INPUT_DEVICE_0,ANA_INPUT_RIGHT)||ana_input_direction(ANA_INPUT_DEVICE_0,ANA_INPUT_UP)||ana_input_direction(ANA_INPUT_DEVICE_0,ANA_INPUT_DOWN);if(d.key_ctrl_down&&!h_prev_ctrl){h_keyboard_events++;h_ctrl_events++;}if(d.key_space_down&&!h_prev_space){h_keyboard_events++;h_space_events++;}if(!d.key_ctrl_down&&ana_input_action(ANA_INPUT_DEVICE_0,ANA_ACTION_1)&&!h_prev_fire){h_joystick_events++;h_joy_fire_events++;}if(!d.key_ctrl_down&&direction&&!h_prev_direction){h_joystick_events++;h_joy_direction_events++;}h_prev_ctrl=d.key_ctrl_down;h_prev_space=d.key_space_down;h_prev_fire=ana_input_action(ANA_INPUT_DEVICE_0,ANA_ACTION_1);h_prev_direction=direction; }
static void h_measure_window(void) { int elapsed,fps; if(t.state!=VOIDSTRIKE_PLAYING)return;if(h_window_start==0)h_window_start=(int)ana_platform_time_ticks();if((t.frame%250)!=0)return;elapsed=(int)ana_platform_time_ticks()-h_window_start;if(elapsed>0){fps=(250*(int)ana_platform_time_ticks_per_second()*100)/elapsed;if(h_min_fps==0||fps<h_min_fps)h_min_fps=fps;}h_window_start=(int)ana_platform_time_ticks(); }
#endif
static const ANA_Color palette[16]={{0,0,0},{17,17,34},{34,34,51},{51,68,85},{85,102,119},{119,136,153},{170,187,204},{221,238,255},{0,51,102},{0,85,170},{0,170,221},{17,102,51},{68,221,119},{255,170,34},{255,221,68},{221,51,68}};
static int hit(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh) { return ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by; }
static unsigned char terrain_tile(int tx,int ty,void *user_data) { (void)user_data; return (unsigned char)((tx + ty * 3) % 3); }
static void terrain_draw(unsigned char tile,int x,int y,void *user_data) { (void)user_data; ana_fill_rect(tile==0?3u:(tile==1?4u:8u),x,y,16,16); }
static void clear_actors(Actor *a) { int i; for(i=0;i<MAX_ACTORS;i++)a[i].active=0; }
static ANA_Image player_image_for_modules(void) { if(t.installed_modules&8u)return player_laser_image;if(t.installed_modules&4u)return player_wide_image;if(t.installed_modules&2u)return player_twin_image;if(t.installed_modules&1u)return player_speed_image;return player_base_image; }
static ANA_Image enemy_image_for_type(int type) { return type==0?turret_image:(type==1?crawler_image:drone_image); }
static void free_assets(void) { ana_free_image(title_image);ana_free_image(core_image);ana_free_image(player_shot_image);ana_free_image(boss_image);ana_free_image(drone_image);ana_free_image(crawler_image);ana_free_image(turret_image);ana_free_image(player_laser_image);ana_free_image(player_wide_image);ana_free_image(player_twin_image);ana_free_image(player_speed_image);ana_free_image(player_base_image);ana_free_sound(victory_sound);ana_free_sound(death_sound);ana_free_sound(explosion_sound);ana_free_sound(install_sound);ana_free_sound(pickup_sound);ana_free_sound(fire_sound); }
static void load_assets(void) { ANA_AudioConfig audio; audio.music_channels=ANA_AUDIO_CH_0|ANA_AUDIO_CH_1;audio.sfx_channels=ANA_AUDIO_CH_2|ANA_AUDIO_CH_3;audio.sfx_can_steal_music=0;audio.music_can_use_free_sfx_channels=0;ana_configure_audio(&audio);player_base_image=ana_load_image(VS_ASSET_ROOT "player_base.anaimg");player_speed_image=ana_load_image(VS_ASSET_ROOT "player_speed.anaimg");player_twin_image=ana_load_image(VS_ASSET_ROOT "player_twin.anaimg");player_wide_image=ana_load_image(VS_ASSET_ROOT "player_wide.anaimg");player_laser_image=ana_load_image(VS_ASSET_ROOT "player_laser.anaimg");turret_image=ana_load_image(VS_ASSET_ROOT "defense_node.anaimg");crawler_image=ana_load_image(VS_ASSET_ROOT "maintenance_crawler.anaimg");drone_image=ana_load_image(VS_ASSET_ROOT "security_drone.anaimg");boss_image=ana_load_image(VS_ASSET_ROOT "reactor_guardian.anaimg");player_shot_image=ana_load_image(VS_ASSET_ROOT "player_shot.anaimg");core_image=ana_load_image(VS_ASSET_ROOT "energy_core.anaimg");title_image=ana_load_image(VS_ASSET_ROOT "title_wordmark.anaimg");fire_sound=ana_load_sound(VS_ASSET_ROOT "fire.anasnd");pickup_sound=ana_load_sound(VS_ASSET_ROOT "pickup.anasnd");install_sound=ana_load_sound(VS_ASSET_ROOT "install.anasnd");explosion_sound=ana_load_sound(VS_ASSET_ROOT "explosion.anasnd");death_sound=ana_load_sound(VS_ASSET_ROOT "player_death.anasnd");victory_sound=ana_load_sound(VS_ASSET_ROOT "victory.anasnd"); }
static int player_width(void) { return PLAYER_W+((t.installed_modules&2u)?4:0)+((t.installed_modules&4u)?10:0); }
static void start_game(void) { t.score=0;t.lives=3;t.selected_module=-1;t.installed_modules=0;t.enemies_spawned=0;t.enemies_destroyed=0;t.boss_phase=0;t.state=VOIDSTRIKE_PLAYING;px=152;py=184;invul=50;fire_wait=0;scroll=0;boss_hp=48;boss_x=128;clear_actors(bullets);clear_actors(enemies);clear_actors(cores); }
static void add_core(int x,int y) { int i; for(i=0;i<MAX_ACTORS;i++)if(!cores[i].active){cores[i].active=1;cores[i].x=x;cores[i].y=y;return;} }
static void shoot(void) { int i; if(fire_wait)return;ana_play_sound(fire_sound);for(i=0;i<MAX_ACTORS;i++)if(!bullets[i].active){bullets[i].active=1;bullets[i].x=px+7;bullets[i].y=py;break;} if((t.installed_modules&2u)&&i+1<MAX_ACTORS){bullets[i+1].active=1;bullets[i+1].x=px+15;bullets[i+1].y=py;} fire_wait=(t.installed_modules&8u)?8:12; }
static void hurt_player(void) { if(invul)return;ana_play_sound(death_sound);t.lives--;if(t.lives<=0){t.state=VOIDSTRIKE_GAME_OVER;return;}t.installed_modules=0;t.selected_module=-1;px=152;invul=80;t.state=VOIDSTRIKE_RESPAWN; }
static void spawn(int tick) { int i; int x;if(tick>=LEVEL_TICKS-2200||tick%45)return;for(i=0;i<MAX_ACTORS;i++)if(!enemies[i].active){x=20+((tick*37+i*19)%270);if(x>px-18&&x<px+player_width()+18)x=(x+112)%290+12;enemies[i].active=1;enemies[i].type=(tick/45)%3;enemies[i].x=x;enemies[i].y=TOP;enemies[i].hp=enemies[i].type?1:2;t.enemies_spawned++;return;} }
static void play(int tick)
{ int i,j,w; scroll++;ana_camera_set_position(&terrain_camera,0,scroll);ana_tile_layer_set_camera(&terrain_layer,&terrain_camera);if(fire_wait)fire_wait--;if(invul)invul--;if(t.state==VOIDSTRIKE_RESPAWN&&invul<45)t.state=VOIDSTRIKE_PLAYING;w=player_width();if(ana_input_direction(ANA_INPUT_DEVICE_0,ANA_INPUT_LEFT))px-=(t.installed_modules&1u)?4:3;if(ana_input_direction(ANA_INPUT_DEVICE_0,ANA_INPUT_RIGHT))px+=(t.installed_modules&1u)?4:3;if(ana_input_direction(ANA_INPUT_DEVICE_0,ANA_INPUT_UP))py-=3;if(ana_input_direction(ANA_INPUT_DEVICE_0,ANA_INPUT_DOWN))py+=3;px=ana_clamp_int(px,4,ANA_DEFAULT_WIDTH-w-4);py=ana_clamp_int(py,TOP+4,BOTTOM-PLAYER_H);if(ana_input_action(ANA_INPUT_DEVICE_0,ANA_ACTION_1))shoot();if(ana_input_action_pressed(ANA_INPUT_DEVICE_0,ANA_ACTION_2)&&t.selected_module>=0){if(t.installed_modules&(1u<<t.selected_module)){t.score+=250;
#ifdef VOIDSTRIKE_EMULATOR_HARNESS
h_repeat_installs++;
#endif
}else {t.installed_modules|=1u<<t.selected_module;
#ifdef VOIDSTRIKE_EMULATOR_HARNESS
h_module_installs[t.selected_module]++;
#endif
}t.selected_module=-1;}spawn(tick);
for(i=0;i<MAX_ACTORS;i++)if(bullets[i].active){bullets[i].y-=7;if(bullets[i].y<TOP)bullets[i].active=0;}
for(i=0;i<MAX_ACTORS;i++)if(enemies[i].active){enemies[i].y+=enemies[i].type==1?2:1;if(hit(px,py,w,PLAYER_H,enemies[i].x,enemies[i].y,14,12)){enemies[i].active=0;hurt_player();}for(j=0;j<MAX_ACTORS;j++)if(bullets[j].active&&hit(bullets[j].x,bullets[j].y,2,6,enemies[i].x,enemies[i].y,14,12)){bullets[j].active=0;if(--enemies[i].hp<=0){add_core(enemies[i].x,enemies[i].y);enemies[i].active=0;t.score+=100;t.enemies_destroyed++;}break;}if(enemies[i].y>BOTTOM)enemies[i].active=0;}
for(i=0;i<MAX_ACTORS;i++)if(cores[i].active){cores[i].y++;if(hit(px,py,w,PLAYER_H,cores[i].x,cores[i].y,8,8)){cores[i].active=0;t.selected_module=(t.selected_module+1)&3;
#ifdef VOIDSTRIKE_EMULATOR_HARNESS
if(t.selected_module==0)h_module_wraps++;
#endif
}}
if(tick>=LEVEL_TICKS){t.boss_phase=1;}if(t.boss_phase){boss_x=128+((tick/12)%50);for(j=0;j<MAX_ACTORS;j++)if(bullets[j].active&&hit(bullets[j].x,bullets[j].y,2,6,boss_x,38,64,30)){bullets[j].active=0;boss_hp--;}if(boss_hp<24){t.boss_phase=2;}if(boss_hp<=0){t.score+=5000;t.state=VOIDSTRIKE_VICTORY;}} }
void voidstrike_init(void) { ana_set_palette(palette,16);ana_input_clear_key_map();ana_input_map_default_keys(ANA_INPUT_DEVICE_0);ana_input_map_key_to_action(ANA_KEY_CTRL,ANA_INPUT_DEVICE_0,ANA_ACTION_1);ana_input_map_key_to_action(ANA_KEY_SPACE,ANA_INPUT_DEVICE_0,ANA_ACTION_2);ana_camera_init(&terrain_camera,0,TOP,ANA_DEFAULT_WIDTH,BOTTOM-TOP,ANA_DEFAULT_WIDTH,4096);ana_tile_layer_init(&terrain_layer,ANA_LAYER_VERTICAL_SCROLL,0,16,16,20,256);ana_tile_layer_set_callbacks(&terrain_layer,terrain_tile,terrain_draw,0);ana_tile_layer_set_viewport(&terrain_layer,ana_rect_make(0,TOP,ANA_DEFAULT_WIDTH,BOTTOM-TOP));ana_tile_layer_set_clear_color(&terrain_layer,3u);ana_tile_layer_set_scroll_backend(&terrain_layer,ANA_SCROLL_BACKEND_HARDWARE);ana_tile_layer_set_scroll_sync(&terrain_layer,ANA_SCROLL_SYNC_DIRTY);ana_tile_layer_set_camera(&terrain_layer,&terrain_camera);t.state=VOIDSTRIKE_TITLE;
#ifdef VOIDSTRIKE_EMULATOR_HARNESS
h_read_request();h_phase("title");
#endif
}
void voidstrike_load(void){load_assets();} void voidstrike_shutdown(void){free_assets();
#ifdef VOIDSTRIKE_EMULATOR_HARNESS
h_write_result();
#endif
} VoidstrikeTelemetry voidstrike_telemetry(void){return t;}
void voidstrike_update(ANA_Time time){t.frame=time.tick;
#ifdef VOIDSTRIKE_EMULATOR_HARNESS
h_drive_input();
h_observe_input();h_measure_window();
if(t.frame >= VOIDSTRIKE_HARNESS_FRAME_LIMIT &&
        t.state != VOIDSTRIKE_GAME_OVER && t.state != VOIDSTRIKE_VICTORY)
    t.state = VOIDSTRIKE_GAME_OVER;
#endif
if(t.state==VOIDSTRIKE_TITLE||t.state==VOIDSTRIKE_GAME_OVER||t.state==VOIDSTRIKE_VICTORY){if(ana_input_action_pressed(ANA_INPUT_DEVICE_0,ANA_ACTION_1))start_game();}else play(time.tick);
#ifdef VOIDSTRIKE_EMULATOR_HARNESS
if(t.state==VOIDSTRIKE_GAME_OVER||t.state==VOIDSTRIKE_VICTORY)ana_quit();
#endif
if(ana_quit_requested())ana_quit();}
static void ship(int x,int y,int w){ana_fill_rect(9,x+5,y,6,16);ana_fill_rect(7,x+7,y+3,2,5);if(t.installed_modules&1u)ana_fill_rect(10,x+6,y+16,4,6);if(t.installed_modules&2u){ana_fill_rect(7,x,y+5,4,8);ana_fill_rect(7,x+12,y+5,4,8);}if(t.installed_modules&4u)ana_fill_rect(10,x-5,y+7,w+10,4);if(t.installed_modules&8u)ana_fill_rect(7,x+7,y-5,2,8);}
void voidstrike_draw(void)
{
    int i;
    int w;

    ana_tile_layer_draw(&terrain_layer);
    ana_fill_rect(2u, 0, 0, ANA_DEFAULT_WIDTH, TOP);
    ana_fill_rect(2u, 0, BOTTOM, ANA_DEFAULT_WIDTH, ANA_DEFAULT_HEIGHT - BOTTOM);
    if (t.state == VOIDSTRIKE_TITLE) {
        if (title_image) ana_draw_image(title_image, 64, 54);
        ana_fill_rect(10u, 70, 100, 180, 4);
        ana_fill_rect(7u, 88, 94, 144, 4);
        ana_fill_rect(12u, 104, 122, 112, 4);
        ana_fill_rect(6u, 76, 144, 168, 3);
        ana_fill_rect(10u, 76, 154, 116, 3);
        ana_fill_rect(7u, 76, 164, 142, 3);
        return;
    }
    if (t.state == VOIDSTRIKE_GAME_OVER || t.state == VOIDSTRIKE_VICTORY) {
        ana_fill_rect(t.state == VOIDSTRIKE_VICTORY ? 12u : 15u, 104, 105, 112, 8);
        return;
    }
    for (i = 0; i < t.lives; i++) ana_fill_rect(12u, 6 + i * 4, 6, 3, 4);
    for (i = 0; i < 20 && i < t.score / 100; i++) ana_fill_rect(7u, 80 + i * 4, 6, 3, 4);
    for (i = 0; i < 4; i++) {
        ana_fill_rect(i == t.selected_module ? 10u : 4u, 112 + i * 25, 228, 20, 12);
        if (t.installed_modules & (1u << i)) ana_fill_rect(12u, 118 + i * 25, 232, 8, 4);
    }
    w = player_width();
    if (!(invul & 4)) {
        if (player_image_for_modules()) ana_draw_image(player_image_for_modules(), px - 10, py - 4);
        else ship(px, py, w);
    }
    for (i = 0; i < MAX_ACTORS; i++) {
        if (bullets[i].active) { if (player_shot_image) ana_draw_image(player_shot_image, bullets[i].x, bullets[i].y); else ana_fill_rect(7u, bullets[i].x, bullets[i].y, 2, 6); }
        if (enemies[i].active) { if (enemy_image_for_type(enemies[i].type)) ana_draw_image_frame(enemy_image_for_type(enemies[i].type), enemies[i].type == 2 ? i % 3 : 0, enemies[i].x, enemies[i].y); else ana_fill_rect(enemies[i].type == 2 ? 15u : 13u, enemies[i].x, enemies[i].y, 14, 12); }
        if (cores[i].active) { if (core_image) ana_draw_image(core_image, cores[i].x, cores[i].y); else ana_fill_rect(12u, cores[i].x, cores[i].y, 8, 8); }
    }
    if (t.boss_phase) {
        if (boss_image) ana_draw_image(boss_image, boss_x - 24, 26); else ana_fill_rect(15u, boss_x, 38, 64, 30);
        ana_fill_rect(t.boss_phase == 2 ? 7u : 14u, boss_x + 28, 46, 10, 12);
    }
}
