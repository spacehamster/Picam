# Picam

A c++ api for the raspberry pi camera.

## Usage
```
./picam_demo x y
  - x is the test mode between 0 and 11 inclusive
  - y is the port mode between 0 and 5 inclusive
```
Currently there are issues with
* `./picam_demo 2 4` os crash
* `./picam_demo 4 4` os crash
* `./picam_demo 5 4` os crash
* `./picam_demo 8 2` can't close camera, need to power off to restart
* `./picam_demo 11 5` can't close camera, need to power off to restart
* `./picam_demo 1 0` can't close camera
* `./picam_demo 1 3` can't close camera
* `./picam_demo 2 1` vcdbg log msg `155773.898: vcos_abort: Halting` vcdbg log assert `155773.857: assert( *p==0xa55a5aa5 ) failed; ../../../../../vcfw/rtos/common/rtos_common_malloc.c::rtos_pool_aligned_free line 205 rev a3d7660`
* `./picam_demo 3 4` vcdbg log msg `153685.522: vcos_abort: Halting` vcdbg log assert	`153685.468: assert( ! (IS_ALIAS_NORMAL(Y) || IS_ALIAS_NORMAL(U) || IS_ALIAS_NORMAL(V)) ) failed; ../../../../../codecs/image/jpeghw/codec/jpe_enc.c::jpe_enc_rectangle line 295 rev a3d7660`
* `./picam_demo 4 1` vcdbg log msg `vcos_abort: Halting` vcdbg log assert	`133862.975: assert( *p==0xa55a5aa5 ) failed; ../../../../../vcfw/rtos/common/rtos_common_malloc.c::rtos_pool_aligned_free line 205 rev a3d7660`
