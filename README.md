intel_pstateでハードウェアP-stateを利用しないよう指定する。
/etc/default/grub から、コマンドライン引数に `intel_pstate=no_hwp` を追加する。
これにより intel_pstate=passive となり、cpufreq の governor が有効になる。この governor による制御の影響を減らすため scaling_governor を userpace に変更する。

これまでHWP_REQUESTでヒントを出していたところ、PERF_CTLから指定する。
これは msr 0x770 を読み取って hwp のオンオフを確認して自動で変更する。
fanfanで実験中 