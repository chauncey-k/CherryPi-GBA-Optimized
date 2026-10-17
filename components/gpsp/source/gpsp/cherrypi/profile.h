/* GPL-2.0-or-later. Optional main-thread statistical profiler. */
void cp_profile_prepare(void);
void cp_profile_start(void);
void cp_profile_translation(unsigned pc, unsigned region, unsigned thumb);
void cp_profile_flush(unsigned region);
void cp_profile_frame(void);
void cp_profile_origin(unsigned caller, unsigned pc, unsigned cache_used);
