# Vulkan compute - minimal app

This is a minimal application running a simple vulkan compute shader.
No complex sync, or stuff:
 - initiialize a vulkan app with somethine, probably first GPU found
 - update buffers
 - wait idle
 - run compute shader
 - wait idle
 - get results


I originally used this repo to manually test a driver implementation, now I use it to validate some shader compilation
bits. Maybe it doesn't work on your machine. Feel free to send an issue or PR if that's the case. I'll look into it
when I have time 😊
