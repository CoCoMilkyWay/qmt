"""站点注册表。加一个站点 = 写一个 <name>.py 暴露 Site 类, 再在这里登记一行。"""

from . import guoren, jquant

SITES = {m.Site.name: m.Site for m in (guoren, jquant)}


def make(name):
    assert name in SITES, f"未知站点 {name}, 可选: {', '.join(sorted(SITES))}"
    return SITES[name]()
