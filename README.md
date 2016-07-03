# Rebera

#### Real-time Bandwidth Estimation and Rate Adaptation

In this project we study interactive video calls between two users, where at least one of the users is connected over a cellular network. It is known that cellular links present highly-varying network bandwidth and packet delays. If the sending rate of the video call exceeds the available bandwidth, the video frames may be excessively delayed, destroying the interactivity of the video call.

In this study, we present Rebera, a cross-layer design of proactive congestion control, video encoding and rate adaptation, to maximize the video transmission rate while keeping the one-way frame delays sufficiently low. Rebera actively measures the available bandwidth in real-time by employing the video frames as packet trains. Using an online linear adaptive filter, Rebera makes a history-based prediction of the future capacity, and determines a bit budget for the video rate adaptation. Rebera uses the hierarchical-P video encoding structure to provide error resilience and to ease rate adaptation, while maintaining low encoding complexity and delay. Furthermore, Rebera decides in real time whether to send or discard an encoded frame, according to the budget, thereby preventing self-congestion and minimizing the packet delays. Our experiments with real cellular link traces demonstrate Rebera can, on average, deliver higher bandwidth utilization and shorter packet delays than Apple's FaceTime.

This research was done in collaboration with Tencent Inc. ([WeChat International](http://www.wechat.com/en/)).

## Reproducing our results

Rebera runs on Linux and uses [x264](http://www.videolan.org/developers/x264.html) library for H.264/AVC video encoding, [ffmpeg](http://ffmpeg.org/) library for decoding, and [SDL2.0](https://www.libsdl.org/download-2.0.php) library for video display purposes, so you need to download these first. 

<code>sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev libsdl2-dev</code>

We specifically left the x264 library out, because the source code needs to be modified so as to generate an AVC stream with hierarchical-P coding structure with 3 temporal layers. You can download the source code [here](http://bit.ly/299X9wU). To find out how we modify the source code, follow [this blog post](https://eymenkurdoglu.github.io/2016/07/01/hierp-one.html). Then you can configure, make and install as usual.


