import os.path

top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')
    ctx.load('autoconfig', tooldir='pebble-autoconfig/wtools')

def configure(ctx):
    ctx.load('pebble_sdk')
    ctx.load('autoconfig', tooldir='pebble-autoconfig/wtools')

def build(ctx):
    ctx.load('pebble_sdk')
    ctx.load('autoconfig', tooldir='pebble-autoconfig/wtools')

    binaries = []

    for p in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[p])
        app_elf='{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_autoconfprogram(source=ctx.path.ant_glob('src/**/*.c'), target=app_elf)
        binaries.append({'platform': p, 'app_elf': app_elf})

    ctx.pbl_bundle(binaries=binaries, js='pebble-js-app.js')
