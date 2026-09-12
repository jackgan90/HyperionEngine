# Performance evidence

Baseline/candidate run sequentially in three A/B pairs per cell. Debug/Release, Forward/Deferred, four motions, CSM 1024, GUI/VSync off, Tracy off, validation unchanged. Each run uses its own frozen binaries/content and the explicitly selected Showcase scene. Warmup 240; measured samples 600. The continuous case uses 3000 measured samples after warmup.

Raw per-frame CSV and complete commands, return codes and all-column mean/P95/minimum/maximum/first/last values: `out/SceneOwnedCamerasAndLights/MainRun/PerformanceMatrixFinal/`. `RawResults.json` retains every run and GPU submission uniqueness checks. `FinalCandidateBinaryHashes.json` records the measured candidate binaries. P95 is the observed sorted sample at floor((N-1)*0.95).

## Individual runs

| Run | rc | samples | frame mean/P95 ms | pipeline mean/P95 ms | ready / unique GPU / zero validation / no geometry update |
| --- | ---: | ---: | --- | --- | --- |
| debug-forward-static-1-baseline | 0 | 600 | 8.5367/11.4519 | 11.2763/14.6478 | True/True/True/None |
| debug-forward-static-1-candidate | 0 | 600 | 7.5179/11.1308 | 10.1246/14.2457 | True/True/True/True |
| debug-forward-static-2-baseline | 0 | 600 | 8.5868/11.4217 | 11.3971/14.6670 | True/True/True/None |
| debug-forward-static-2-candidate | 0 | 600 | 7.6852/11.0974 | 10.3229/14.3224 | True/True/True/True |
| debug-forward-static-3-baseline | 0 | 600 | 8.8644/11.2808 | 11.7918/14.6524 | True/True/True/None |
| debug-forward-static-3-candidate | 0 | 600 | 7.9420/11.1715 | 10.5700/14.1995 | True/True/True/True |
| debug-forward-small-camera-1-baseline | 0 | 600 | 16.1038/19.9671 | 21.7748/26.8992 | True/True/True/None |
| debug-forward-small-camera-1-candidate | 0 | 600 | 14.0179/16.5263 | 19.1265/22.1845 | True/True/True/True |
| debug-forward-small-camera-2-baseline | 0 | 600 | 14.2212/16.9678 | 19.3383/22.5575 | True/True/True/None |
| debug-forward-small-camera-2-candidate | 0 | 600 | 14.1725/16.5467 | 19.3276/22.0195 | True/True/True/True |
| debug-forward-small-camera-3-baseline | 0 | 600 | 14.1512/16.8902 | 19.2816/22.3108 | True/True/True/None |
| debug-forward-small-camera-3-candidate | 0 | 600 | 14.5353/17.6497 | 19.6910/23.6734 | True/True/True/True |
| debug-forward-large-camera-1-baseline | 0 | 600 | 17.5761/21.0372 | 23.8113/28.4384 | True/True/True/None |
| debug-forward-large-camera-1-candidate | 0 | 600 | 18.3991/22.0715 | 24.8065/29.1092 | True/True/True/True |
| debug-forward-large-camera-2-baseline | 0 | 600 | 17.5603/20.3634 | 23.8026/27.0165 | True/True/True/None |
| debug-forward-large-camera-2-candidate | 0 | 600 | 17.6104/20.0262 | 23.8458/26.8595 | True/True/True/True |
| debug-forward-large-camera-3-baseline | 0 | 600 | 17.8787/21.2052 | 24.2295/27.9663 | True/True/True/None |
| debug-forward-large-camera-3-candidate | 0 | 600 | 17.7818/20.6190 | 23.9775/27.1253 | True/True/True/True |
| debug-forward-moving-light-1-baseline | 0 | 600 | 13.9218/16.3491 | 19.2654/22.2188 | True/True/True/None |
| debug-forward-moving-light-1-candidate | 0 | 600 | 14.2770/16.6916 | 19.9202/22.8103 | True/True/True/True |
| debug-forward-moving-light-2-baseline | 0 | 600 | 13.8565/17.2012 | 19.2277/23.5440 | True/True/True/None |
| debug-forward-moving-light-2-candidate | 0 | 600 | 14.5968/17.0899 | 20.3685/23.5969 | True/True/True/True |
| debug-forward-moving-light-3-baseline | 0 | 600 | 13.6070/15.9769 | 18.8645/21.6156 | True/True/True/None |
| debug-forward-moving-light-3-candidate | 0 | 600 | 15.7087/18.8836 | 21.8175/26.1526 | True/True/True/True |
| debug-deferred-static-1-baseline | 0 | 600 | 10.6744/12.4285 | 13.1889/14.9491 | True/True/True/None |
| debug-deferred-static-1-candidate | 0 | 600 | 11.2192/13.5118 | 13.8173/16.1787 | True/True/True/True |
| debug-deferred-static-2-baseline | 0 | 600 | 10.8658/12.4657 | 13.2465/14.8359 | True/True/True/None |
| debug-deferred-static-2-candidate | 0 | 600 | 10.8682/12.4458 | 13.5972/15.1148 | True/True/True/True |
| debug-deferred-static-3-baseline | 0 | 600 | 10.9541/12.8451 | 13.3579/14.9884 | True/True/True/None |
| debug-deferred-static-3-candidate | 0 | 600 | 10.8978/12.8176 | 13.4195/15.0735 | True/True/True/True |
| debug-deferred-small-camera-1-baseline | 0 | 600 | 15.8632/17.7957 | 22.3795/25.1771 | True/True/True/None |
| debug-deferred-small-camera-1-candidate | 0 | 600 | 16.8901/18.9133 | 23.6520/26.4034 | True/True/True/True |
| debug-deferred-small-camera-2-baseline | 0 | 600 | 16.1990/18.1916 | 22.8738/25.5538 | True/True/True/None |
| debug-deferred-small-camera-2-candidate | 0 | 600 | 16.5137/18.6476 | 23.2195/26.3609 | True/True/True/True |
| debug-deferred-small-camera-3-baseline | 0 | 600 | 16.1308/18.3113 | 22.7238/25.4647 | True/True/True/None |
| debug-deferred-small-camera-3-candidate | 0 | 600 | 17.0542/19.7245 | 24.0002/27.3059 | True/True/True/True |
| debug-deferred-large-camera-1-baseline | 0 | 600 | 19.2638/21.9537 | 27.0343/30.6737 | True/True/True/None |
| debug-deferred-large-camera-1-candidate | 0 | 600 | 20.1975/23.9216 | 28.2525/33.1181 | True/True/True/True |
| debug-deferred-large-camera-2-baseline | 0 | 600 | 19.3063/22.3390 | 27.0566/30.4925 | True/True/True/None |
| debug-deferred-large-camera-2-candidate | 0 | 600 | 19.9761/23.4892 | 27.9108/32.7296 | True/True/True/True |
| debug-deferred-large-camera-3-baseline | 0 | 600 | 18.9503/21.4231 | 26.6209/29.6377 | True/True/True/None |
| debug-deferred-large-camera-3-candidate | 0 | 600 | 19.9907/22.9798 | 27.8979/31.4428 | True/True/True/True |
| debug-deferred-moving-light-1-baseline | 0 | 600 | 13.3204/15.0984 | 19.1966/21.4282 | True/True/True/None |
| debug-deferred-moving-light-1-candidate | 0 | 600 | 15.6482/22.2824 | 22.5817/31.3905 | True/True/True/True |
| debug-deferred-moving-light-2-baseline | 0 | 600 | 11.8495/14.0492 | 16.7925/19.5816 | True/True/True/None |
| debug-deferred-moving-light-2-candidate | 0 | 600 | 12.7833/15.0583 | 18.2577/20.8584 | True/True/True/True |
| debug-deferred-moving-light-3-baseline | 0 | 600 | 13.2338/14.8707 | 18.9984/20.9560 | True/True/True/None |
| debug-deferred-moving-light-3-candidate | 0 | 600 | 14.2762/16.2478 | 20.6020/23.1356 | True/True/True/True |
| release-forward-static-1-baseline | 0 | 600 | 1.2646/2.2366 | 0.1969/0.3236 | True/True/True/None |
| release-forward-static-1-candidate | 0 | 600 | 1.3909/2.5221 | 0.2140/0.3773 | True/True/True/True |
| release-forward-static-2-baseline | 0 | 600 | 1.4671/3.1661 | 0.2040/0.3444 | True/True/True/None |
| release-forward-static-2-candidate | 0 | 600 | 1.5335/3.2809 | 0.2074/0.3710 | True/True/True/True |
| release-forward-static-3-baseline | 0 | 600 | 1.3792/3.1992 | 0.1979/0.3291 | True/True/True/None |
| release-forward-static-3-candidate | 0 | 600 | 1.3095/2.6591 | 0.1683/0.2322 | True/True/True/True |
| release-forward-small-camera-1-baseline | 0 | 600 | 1.5525/2.3838 | 0.8774/1.1601 | True/True/True/None |
| release-forward-small-camera-1-candidate | 0 | 600 | 1.6525/2.6247 | 0.9807/1.3338 | True/True/True/True |
| release-forward-small-camera-2-baseline | 0 | 600 | 1.8780/3.8890 | 0.9738/1.2768 | True/True/True/None |
| release-forward-small-camera-2-candidate | 0 | 600 | 1.8748/3.7057 | 1.0317/1.4337 | True/True/True/True |
| release-forward-small-camera-3-baseline | 0 | 600 | 1.6123/2.8832 | 0.9236/1.2214 | True/True/True/None |
| release-forward-small-camera-3-candidate | 0 | 600 | 1.4973/2.2908 | 0.9228/1.1816 | True/True/True/True |
| release-forward-large-camera-1-baseline | 0 | 600 | 1.6814/2.5595 | 1.2139/1.5753 | True/True/True/None |
| release-forward-large-camera-1-candidate | 0 | 600 | 1.8242/2.8210 | 1.3153/1.8173 | True/True/True/True |
| release-forward-large-camera-2-baseline | 0 | 600 | 1.6358/2.4959 | 1.1448/1.4901 | True/True/True/None |
| release-forward-large-camera-2-candidate | 0 | 600 | 1.6938/2.4915 | 1.2144/1.5540 | True/True/True/True |
| release-forward-large-camera-3-baseline | 0 | 600 | 1.5818/2.4303 | 1.1191/1.3883 | True/True/True/None |
| release-forward-large-camera-3-candidate | 0 | 600 | 1.8967/3.8069 | 1.1985/1.5738 | True/True/True/True |
| release-forward-moving-light-1-baseline | 0 | 600 | 1.6720/3.3514 | 0.8130/1.0004 | True/True/True/None |
| release-forward-moving-light-1-candidate | 0 | 600 | 1.5810/2.4498 | 0.9399/1.1748 | True/True/True/True |
| release-forward-moving-light-2-baseline | 0 | 600 | 1.4846/2.3778 | 0.8577/1.0927 | True/True/True/None |
| release-forward-moving-light-2-candidate | 0 | 600 | 1.6412/2.6363 | 0.9848/1.3371 | True/True/True/True |
| release-forward-moving-light-3-baseline | 0 | 600 | 1.5662/2.7839 | 0.8882/1.1392 | True/True/True/None |
| release-forward-moving-light-3-candidate | 0 | 600 | 1.4715/2.2690 | 0.8674/1.0685 | True/True/True/True |
| release-deferred-static-1-baseline | 0 | 600 | 1.3208/2.2942 | 0.3332/0.4573 | True/True/True/None |
| release-deferred-static-1-candidate | 0 | 600 | 1.4981/2.7407 | 0.2885/0.4175 | True/True/True/True |
| release-deferred-static-2-baseline | 0 | 600 | 1.6555/3.8768 | 0.3462/0.4718 | True/True/True/None |
| release-deferred-static-2-candidate | 0 | 600 | 1.4144/2.2058 | 0.3048/0.3742 | True/True/True/True |
| release-deferred-static-3-baseline | 0 | 600 | 1.3754/2.1051 | 0.2815/0.3392 | True/True/True/None |
| release-deferred-static-3-candidate | 0 | 600 | 1.3777/2.4602 | 0.3460/0.4919 | True/True/True/True |
| release-deferred-small-camera-1-baseline | 0 | 600 | 1.9192/3.7516 | 1.0691/1.4590 | True/True/True/None |
| release-deferred-small-camera-1-candidate | 0 | 600 | 1.9792/3.7030 | 1.0827/1.3917 | True/True/True/True |
| release-deferred-small-camera-2-baseline | 0 | 600 | 1.9640/4.3104 | 0.9705/1.2279 | True/True/True/None |
| release-deferred-small-camera-2-candidate | 0 | 600 | 1.9948/3.8686 | 1.0396/1.3467 | True/True/True/True |
| release-deferred-small-camera-3-baseline | 0 | 600 | 1.8949/3.7385 | 0.9613/1.2606 | True/True/True/None |
| release-deferred-small-camera-3-candidate | 0 | 600 | 1.9563/3.3381 | 1.1487/1.5262 | True/True/True/True |
| release-deferred-large-camera-1-baseline | 0 | 600 | 1.9514/3.6843 | 1.1503/1.4384 | True/True/True/None |
| release-deferred-large-camera-1-candidate | 0 | 600 | 2.0070/3.8477 | 1.1909/1.5590 | True/True/True/True |
| release-deferred-large-camera-2-baseline | 0 | 600 | 1.9916/3.8650 | 1.2119/1.6142 | True/True/True/None |
| release-deferred-large-camera-2-candidate | 0 | 600 | 1.9580/3.5631 | 1.1640/1.4948 | True/True/True/True |
| release-deferred-large-camera-3-baseline | 0 | 600 | 1.8826/3.4226 | 1.1930/1.4946 | True/True/True/None |
| release-deferred-large-camera-3-candidate | 0 | 600 | 1.9715/3.0171 | 1.3059/1.7430 | True/True/True/True |
| release-deferred-moving-light-1-baseline | 0 | 600 | 1.7621/3.1261 | 0.8287/1.0982 | True/True/True/None |
| release-deferred-moving-light-1-candidate | 0 | 600 | 1.9736/3.3117 | 0.9083/1.1807 | True/True/True/True |
| release-deferred-moving-light-2-baseline | 0 | 600 | 1.9184/4.0054 | 0.8147/1.0477 | True/True/True/None |
| release-deferred-moving-light-2-candidate | 0 | 600 | 2.0595/4.9996 | 0.8823/1.1381 | True/True/True/True |
| release-deferred-moving-light-3-baseline | 0 | 600 | 1.9920/4.2726 | 0.8738/1.2075 | True/True/True/None |
| release-deferred-moving-light-3-candidate | 0 | 600 | 1.8393/3.6731 | 0.7877/0.9908 | True/True/True/True |
| debug-forward-continuous-camera-light-1-candidate | 0 | 3000 | 18.3640/21.8955 | 24.6264/28.9057 | True/True/True/True |

## Median of three runs: CPU/GPU timings

Each entry is median(mean)/median(P95) in milliseconds. Camera/light-only BVH counters are candidate-only additions; baseline missing counters are unavailable, not zero.

| Cell/version | frame_ms | pipeline_prepare_ms | material_ms | plan_ms | prepare_ms |
| --- | --- | --- | --- | --- | --- |
| debug/forward/static/baseline | 8.5868/11.4217 | 11.3971/14.6524 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 |
| debug/forward/static/candidate | 7.6852/11.1308 | 10.3229/14.2457 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 |
| debug/forward/small-camera/baseline | 14.2212/16.9678 | 19.3383/22.5575 | 2.6465/3.2536 | 0.2715/0.6749 | 0.7404/1.0200 |
| debug/forward/small-camera/candidate | 14.1725/16.5467 | 19.3276/22.1845 | 2.5970/3.1856 | 0.2716/0.6815 | 0.6977/0.9850 |
| debug/forward/large-camera/baseline | 17.5761/21.0372 | 23.8113/27.9663 | 3.2338/4.0466 | 0.8995/1.4554 | 0.8807/1.1586 |
| debug/forward/large-camera/candidate | 17.7818/20.6190 | 23.9775/27.1253 | 3.1278/3.9481 | 0.8946/1.4225 | 0.8567/1.1359 |
| debug/forward/moving-light/baseline | 13.8565/16.3491 | 19.2277/22.2188 | 3.0584/4.0846 | 0.1438/0.1974 | 0.7629/1.0358 |
| debug/forward/moving-light/candidate | 14.5968/17.0899 | 20.3685/23.5969 | 3.0630/4.1599 | 0.1440/0.1962 | 0.7445/1.0206 |
| debug/deferred/static/baseline | 10.8658/12.4657 | 13.2465/14.9491 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 |
| debug/deferred/static/candidate | 10.8978/12.8176 | 13.5972/15.1148 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 |
| debug/deferred/small-camera/baseline | 16.1308/18.1916 | 22.7238/25.4647 | 2.1355/2.5301 | 0.2646/0.6421 | 0.4970/0.7545 |
| debug/deferred/small-camera/candidate | 16.8901/18.9133 | 23.6520/26.4034 | 2.1367/2.6250 | 0.2627/0.6415 | 0.4830/0.7540 |
| debug/deferred/large-camera/baseline | 19.2638/21.9537 | 27.0343/30.4925 | 2.5705/3.2833 | 0.8585/1.4135 | 0.6359/0.8974 |
| debug/deferred/large-camera/candidate | 19.9907/23.4892 | 27.9108/32.7296 | 2.7234/3.4219 | 0.8615/1.4395 | 0.6422/0.8744 |
| debug/deferred/moving-light/baseline | 13.2338/14.8707 | 18.9984/20.9560 | 1.9853/3.1160 | 0.0318/0.0502 | 0.0117/0.0167 |
| debug/deferred/moving-light/candidate | 14.2762/16.2478 | 20.6020/23.1356 | 2.0940/3.1969 | 0.0332/0.0523 | 0.0133/0.0212 |
| release/forward/static/baseline | 1.3792/3.1661 | 0.1979/0.3291 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 |
| release/forward/static/candidate | 1.3909/2.6591 | 0.2074/0.3710 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 |
| release/forward/small-camera/baseline | 1.6123/2.8832 | 0.9236/1.2214 | 0.1327/0.1873 | 0.0258/0.0740 | 0.0451/0.0681 |
| release/forward/small-camera/candidate | 1.6525/2.6247 | 0.9807/1.3338 | 0.1439/0.2062 | 0.0297/0.0921 | 0.0491/0.0747 |
| release/forward/large-camera/baseline | 1.6358/2.4959 | 1.1448/1.4901 | 0.1495/0.2138 | 0.0673/0.1129 | 0.0542/0.0820 |
| release/forward/large-camera/candidate | 1.8242/2.8210 | 1.2144/1.5738 | 0.1597/0.2301 | 0.0731/0.1251 | 0.0561/0.0861 |
| release/forward/moving-light/baseline | 1.5662/2.7839 | 0.8577/1.0927 | 0.1359/0.2002 | 0.0096/0.0139 | 0.0474/0.0660 |
| release/forward/moving-light/candidate | 1.5810/2.4498 | 0.9399/1.1748 | 0.1495/0.2295 | 0.0107/0.0146 | 0.0510/0.0703 |
| release/deferred/static/baseline | 1.3754/2.2942 | 0.3332/0.4573 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 |
| release/deferred/static/candidate | 1.4144/2.4602 | 0.3048/0.4175 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 |
| release/deferred/small-camera/baseline | 1.9192/3.7516 | 0.9705/1.2606 | 0.0996/0.1393 | 0.0254/0.0804 | 0.0301/0.0487 |
| release/deferred/small-camera/candidate | 1.9792/3.7030 | 1.0827/1.3917 | 0.1066/0.1515 | 0.0272/0.0838 | 0.0328/0.0528 |
| release/deferred/large-camera/baseline | 1.9514/3.6843 | 1.1930/1.4946 | 0.1093/0.1534 | 0.0611/0.1039 | 0.0396/0.0655 |
| release/deferred/large-camera/candidate | 1.9715/3.5631 | 1.1909/1.5590 | 0.1151/0.1753 | 0.0640/0.1072 | 0.0406/0.0704 |
| release/deferred/moving-light/baseline | 1.9184/4.0054 | 0.8287/1.0982 | 0.0904/0.1637 | 0.0019/0.0025 | 0.0006/0.0008 |
| release/deferred/moving-light/candidate | 1.9736/3.6731 | 0.8823/1.1381 | 0.0930/0.1566 | 0.0020/0.0026 | 0.0005/0.0008 |

| Cell/version | shadow_setup_ms | shadow_material_ms | shadow_plan_ms | shadow_prepare_ms | total_gpu_pass_ms |
| --- | --- | --- | --- | --- | --- |
| debug/forward/static/baseline | 0.0486/0.0586 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 | 0.1125/0.1120 |
| debug/forward/static/candidate | 0.0414/0.0504 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 | 0.1171/0.1122 |
| debug/forward/small-camera/baseline | 0.6520/0.7611 | 1.3015/1.6095 | 0.5120/0.8582 | 1.4031/1.7823 | 0.1114/0.1131 |
| debug/forward/small-camera/candidate | 0.6415/0.7383 | 1.2468/1.5512 | 0.4893/0.8434 | 1.5073/1.8805 | 0.1130/0.1184 |
| debug/forward/large-camera/baseline | 0.6865/0.7885 | 1.6775/2.2824 | 1.2061/1.7974 | 1.8697/2.2722 | 0.1146/0.1171 |
| debug/forward/large-camera/candidate | 0.6756/0.7796 | 1.6272/2.1671 | 1.1832/1.7542 | 1.9878/2.4262 | 0.1160/0.1183 |
| debug/forward/moving-light/baseline | 0.7238/0.8727 | 1.5128/2.7564 | 0.5491/0.9066 | 1.5066/1.9541 | 0.1150/0.1132 |
| debug/forward/moving-light/candidate | 0.7184/0.8996 | 1.5445/2.8049 | 0.5436/0.8896 | 1.7112/2.1838 | 0.1175/0.1130 |
| debug/deferred/static/baseline | 0.0479/0.0570 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 | 0.1242/0.1261 |
| debug/deferred/static/candidate | 0.0442/0.0556 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 | 0.1254/0.1263 |
| debug/deferred/small-camera/baseline | 0.6587/0.7736 | 1.3149/1.5899 | 0.5289/0.8888 | 1.4553/1.8311 | 0.1286/0.1281 |
| debug/deferred/small-camera/candidate | 0.6663/0.7922 | 1.3237/1.6844 | 0.5283/0.9064 | 1.5695/1.9768 | 0.1267/0.1283 |
| debug/deferred/large-camera/baseline | 0.6895/0.8281 | 1.6492/2.2402 | 1.2557/1.9629 | 1.9030/2.3115 | 0.1311/0.1338 |
| debug/deferred/large-camera/candidate | 0.6983/0.8498 | 1.7839/2.4168 | 1.2791/1.9487 | 2.0765/2.6628 | 0.1318/0.1343 |
| debug/deferred/moving-light/baseline | 0.7200/0.8232 | 1.4951/2.7768 | 0.5556/0.8616 | 1.4490/1.7882 | 0.1267/0.1272 |
| debug/deferred/moving-light/candidate | 0.7370/0.8971 | 1.5889/2.8116 | 0.5690/0.8919 | 1.6758/2.2645 | 0.1282/0.1276 |
| release/forward/static/baseline | 0.0043/0.0051 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 | 0.1104/0.1100 |
| release/forward/static/candidate | 0.0040/0.0046 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 | 0.1079/0.1101 |
| release/forward/small-camera/baseline | 0.1143/0.1369 | 0.0608/0.0845 | 0.0298/0.0756 | 0.0917/0.1280 | 0.1143/0.1103 |
| release/forward/small-camera/candidate | 0.1177/0.1493 | 0.0651/0.0991 | 0.0329/0.0834 | 0.0996/0.1426 | 0.1146/0.1100 |
| release/forward/large-camera/baseline | 0.1145/0.1385 | 0.0723/0.1102 | 0.0732/0.1315 | 0.1170/0.1624 | 0.1175/0.1141 |
| release/forward/large-camera/candidate | 0.1188/0.1437 | 0.0773/0.1219 | 0.0790/0.1535 | 0.1221/0.1670 | 0.1103/0.1149 |
| release/forward/moving-light/baseline | 0.1147/0.1362 | 0.0667/0.1258 | 0.0333/0.0754 | 0.0964/0.1361 | 0.1206/0.1182 |
| release/forward/moving-light/candidate | 0.1184/0.1494 | 0.0747/0.1422 | 0.0348/0.0744 | 0.1041/0.1499 | 0.1251/0.1184 |
| release/deferred/static/baseline | 0.0042/0.0049 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 | 0.1254/0.1260 |
| release/deferred/static/candidate | 0.0036/0.0042 | 0.0000/0.0000 | 0.0000/0.0000 | 0.0000/0.0000 | 0.1264/0.1264 |
| release/deferred/small-camera/baseline | 0.1158/0.1405 | 0.0633/0.0908 | 0.0320/0.0811 | 0.0942/0.1302 | 0.1307/0.1254 |
| release/deferred/small-camera/candidate | 0.1218/0.1545 | 0.0669/0.0969 | 0.0338/0.0832 | 0.1011/0.1392 | 0.1245/0.1254 |
| release/deferred/large-camera/baseline | 0.1182/0.1422 | 0.0728/0.1042 | 0.0766/0.1371 | 0.1194/0.1654 | 0.1261/0.1307 |
| release/deferred/large-camera/candidate | 0.1193/0.1481 | 0.0750/0.1167 | 0.0806/0.1454 | 0.1221/0.1701 | 0.1264/0.1316 |
| release/deferred/moving-light/baseline | 0.1208/0.1484 | 0.0724/0.1429 | 0.0358/0.0832 | 0.1028/0.1479 | 0.1271/0.1256 |
| release/deferred/moving-light/candidate | 0.1184/0.1437 | 0.0735/0.1391 | 0.0377/0.0878 | 0.1036/0.1494 | 0.1222/0.1246 |

## Review trigger lines

| Cell | frame mean % | frame P95 % | pipeline mean % | pipeline P95 % | trigger |
| --- | ---: | ---: | ---: | ---: | --- |
| debug/forward/static | -10.50 | -2.55 | -9.43 | -2.78 | none |
| debug/forward/small-camera | -0.34 | -2.48 | -0.06 | -1.65 | none |
| debug/forward/large-camera | +1.17 | -1.99 | +0.70 | -3.01 | none |
| debug/forward/moving-light | +5.34 | +4.53 | +5.93 | +6.20 | frame_ms_mean, pipeline_prepare_ms_mean |
| debug/deferred/static | +0.29 | +2.82 | +2.65 | +1.11 | none |
| debug/deferred/small-camera | +4.71 | +3.97 | +4.08 | +3.69 | none |
| debug/deferred/large-camera | +3.77 | +6.99 | +3.24 | +7.34 | none |
| debug/deferred/moving-light | +7.88 | +9.26 | +8.44 | +10.40 | frame_ms_mean, pipeline_prepare_ms_mean, pipeline_prepare_ms_p95 |
| release/forward/static | +0.85 | -16.01 | +4.80 | +12.73 | pipeline_prepare_ms_p95 |
| release/forward/small-camera | +2.49 | -8.97 | +6.19 | +9.20 | pipeline_prepare_ms_mean |
| release/forward/large-camera | +11.51 | +13.03 | +6.08 | +5.62 | frame_ms_mean, frame_ms_p95, pipeline_prepare_ms_mean |
| release/forward/moving-light | +0.95 | -12.00 | +9.58 | +7.51 | pipeline_prepare_ms_mean |
| release/deferred/static | +2.83 | +7.24 | -8.53 | -8.70 | none |
| release/deferred/small-camera | +3.13 | -1.30 | +11.56 | +10.40 | pipeline_prepare_ms_mean, pipeline_prepare_ms_p95 |
| release/deferred/large-camera | +1.03 | -3.29 | -0.17 | +4.31 | none |
| release/deferred/moving-light | +2.88 | -8.30 | +6.47 | +3.63 | pipeline_prepare_ms_mean |

## Resource bounds and reuse

Ranges below combine all samples of each candidate cell. Counters describe their CSV semantics; cumulative allocation counters need first/last inspection to assess stability.

| Cell | visible items | scene draws | shadow draws | GPU bytes | descriptors | PSOs | rebuilds/refits max | local packets range |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| debug/forward/static | 185..185 | 6..6 | 24..24 | 3.58769e+07..3.58769e+07 | 115..115 | 5..5 | 0.0/0.0 | 0..0 |
| debug/forward/small-camera | 185..188 | 6..6 | 24..24 | 3.58769e+07..3.6139e+07 | 115..115 | 5..5 | 0.0/0.0 | 0..5 |
| debug/forward/large-camera | 157..187 | 6..6 | 24..24 | 3.58769e+07..3.81706e+07 | 115..115 | 5..5 | 0.0/0.0 | 0..3 |
| debug/forward/moving-light | 185..185 | 6..6 | 23..24 | 3.58769e+07..3.76463e+07 | 115..115 | 5..5 | 0.0/0.0 | 3..5 |
| debug/deferred/static | 185..185 | 6..6 | 24..24 | 6.24845e+07..6.24845e+07 | 108..108 | 6..6 | 0.0/0.0 | 0..0 |
| debug/deferred/small-camera | 185..188 | 6..6 | 24..24 | 6.24845e+07..6.28777e+07 | 108..108 | 6..6 | 0.0/0.0 | 0..5 |
| debug/deferred/large-camera | 157..187 | 6..6 | 24..24 | 6.24845e+07..6.48438e+07 | 108..108 | 6..6 | 0.0/0.0 | 0..3 |
| debug/deferred/moving-light | 185..185 | 6..6 | 23..24 | 6.24845e+07..6.40573e+07 | 108..108 | 6..6 | 0.0/0.0 | 2..4 |
| release/forward/static | 185..185 | 6..6 | 24..24 | 3.58769e+07..3.58769e+07 | 115..115 | 5..5 | 0.0/0.0 | 0..0 |
| release/forward/small-camera | 185..188 | 6..6 | 24..24 | 3.58769e+07..3.62045e+07 | 115..115 | 5..5 | 0.0/0.0 | 0..5 |
| release/forward/large-camera | 157..187 | 6..6 | 24..24 | 3.58769e+07..3.81706e+07 | 115..115 | 5..5 | 0.0/0.0 | 0..3 |
| release/forward/moving-light | 185..185 | 6..6 | 23..24 | 3.58769e+07..3.76463e+07 | 115..115 | 5..5 | 0.0/0.0 | 3..5 |
| release/deferred/static | 185..185 | 6..6 | 24..24 | 6.24845e+07..6.24845e+07 | 108..108 | 6..6 | 0.0/0.0 | 0..0 |
| release/deferred/small-camera | 185..188 | 6..6 | 24..24 | 6.24845e+07..6.29432e+07 | 108..108 | 6..6 | 0.0/0.0 | 0..5 |
| release/deferred/large-camera | 157..187 | 6..6 | 24..24 | 6.24845e+07..6.48438e+07 | 108..108 | 6..6 | 0.0/0.0 | 0..3 |
| release/deferred/moving-light | 185..185 | 6..6 | 23..24 | 6.24845e+07..6.41229e+07 | 108..108 | 6..6 | 0.0/0.0 | 2..4 |

## Continuous camera and light updates

Run `debug-forward-continuous-camera-light-1-candidate`: rc=0, samples=3000, unique GPU IDs=True, ready=True, zero validation=True, no geometry updates=True.

| Metric | first | last | minimum | maximum |
| --- | ---: | ---: | ---: | ---: |
| gpu_allocation_bytes | 3.58769e+07 | 4.37412e+07 | 3.58769e+07 | 4.37412e+07 |
| descriptor_allocations | 115 | 115 | 115 | 115 |
| pipelines_created | 5 | 5 | 5 | 5 |
| constant_bytes_written | 111104 | 6.01771e+06 | 111104 | 6.01771e+06 |
| instance_upload_bytes | 0 | 0 | 0 | 10320 |
| index_rebuilds | 0 | 0 | 0 | 0 |
| index_refits | 0 | 0 | 0 | 0 |
| local_packet_reuses | 3 | 3 | 1 | 5 |
| cached_plan_items | 766 | 778 | 765 | 796 |
| cached_plan_blocks | 30 | 30 | 30 | 30 |
| batch_cached_inputs | 779 | 912 | 779 | 912 |

## Interpretation

Trigger analysis and limitations are recorded in review-handoff.md after inspecting all results. No slow samples or image-threshold failures are removed.
