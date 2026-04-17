intel_pstateでハードウェアP-stateを利用しないよう指定する。
/etc/default/grub から、コマンドライン引数に `intel_pstate=no_hwp` を追加する。

これまでHWP_REQUESTでヒントを出していたところ、PERF_CTLから指定する。
これは msr 0x770 を読み取って hwp のオンオフを確認して自動で変更する。
fanfanで実験中 