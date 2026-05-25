#ifndef ANA_PTPLAYER_H
#define ANA_PTPLAYER_H

#ifdef ANA_TARGET_AMIGA

int ana_pt_install(void);
void ana_pt_remove(void);
void ana_pt_init(void* module);
void ana_pt_end(void);
void ana_pt_enable(int enable);
void ana_pt_mastervol(int volume);
void ana_pt_musicmask(int mask);
void ana_pt_channelmask(int mask);
void* ana_pt_playfx(void* sfx);
void ana_pt_stopfx(int channel);

#endif

#endif
