Configuring the Contiki part of pTunes that runs on the sensor nodes and on the sink is currently more difficult than it should be. Here are a few tips that might be helpful:

* Most of the configuration settings are in `/ptunes/contiki/platform/sky/contiki-conf.h`, including the choice of MAC protocol (X-MAC or LPP) and the initial MAC protocol parameters.

 * We strongly recommend to leave the default `#define GLOSSY 1` to use fast Glossy network floods for collecting network state and disseminating optimized MAC parameters.

 * If you want to simulate pTunes using a recent version of the Cooja network simulator, make sure you select the multi-path ray-tracer medium (MRM) radio model when setting up your simulation environment and `#define COOJA 1`.

 * By default, the nodes build and maintain a dynamic routing tree for collecting data at the sink. The node ID of the sink defaults to `#define SINK_ID 200`. So, make sure your (simulated or deployed) network contains a node with that `SINK_ID`; otherwise, you won't see any packets at the sink because nodes do not know where to route them to.

* Sometimes it may be useful to use a static routing tree, for example, when testing the code or comparing the performance across different runs in which the topology should be the same. To achieve this, `#define STATIC 1` in `/ptunes/contiki/core/net/rime/relcollect.h` and `#define COLLECT_ANNOUNCEMENTS 0` in `/ptunes/contiki/core/net/rime/relcollect.h`, then define your topology in `relcollect_open()` within the latter file. You already find there the definition of the static topology we used in some of the experiments for the IPSN paper.