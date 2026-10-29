/* Shared by the app tick and guide audio regressions. */
{static int last_volume=-1,last_active,was_muted;int active=video_ativo();
  int preview=player_mini_no_guia_ativo()&&g5ui_active();
  int muted=preview&&g5ui_preview_sem_som();
  if(active&&(preview||was_muted)&&(last_volume!=muted||!last_active)){
    video_volume(muted?0:100);last_volume=muted;was_muted=muted;
  }
  if(!active){last_volume=-1;was_muted=0;}last_active=active;
}
