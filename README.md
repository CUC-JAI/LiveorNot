# 🧪 liveornot

> 一句话：一键检查 **Debian 软件源是否还活着**  
> 如果源“挂了”，自动告诉你 **是站点维护、还是网络问题**，并给出下一步怎么做。

---

## ✅ 它能干嘛

| 场景 | 它会做什么 |
|---|---|
| 当前 `apt update` 正常 | 直接报告“源正常”，退出 |
| 源失效 | 批量测试 **Tuna / USTC / 官方** 镜像站 |
| 镜像站返回 **man.xml** | 打印维护公告，告诉你“别慌，在维护” |
| 所有镜像站都 404/500 | 分层检测 **DNS 53 / HTTP 80 / HTTPS 443**<br>明确指出是 **网络被封**、**防火墙** 还是 **镜像站集体宕机** |
| 网络层也跪了 | 给出排查清单：DNS、代理、防火墙、网线 … |

---

## 🚀 一键使用

```bash
# 1. 下载最新二进制（Linux x86_64）
curl -LO https://github.com/YOUR_USER/liveornot/releases/latest/download/liveornot-linux-amd64
chmod +x liveornot-linux-amd64

# 2. 跑
sudo ./liveornot-linux-amd64
```

---

## 🛠️ 自己编译

依赖：**gcc** + **libcurl4-openssl-dev**

```bash
sudo apt update && sudo apt install -y gcc libcurl4-openssl-dev
gcc -O2 -o liveornot liveornot.c -lcurl
sudo ./liveornot
```

---

## 📋 返回码含义

| 码 | 含义 |
|---|---|
| 0 | 当前源正常 |
| 1 | 当前源失效，但发现可用镜像站 |
| 2 | 网络或全部镜像站不可用 |

---

## 🧩 集成到脚本

```bash
sudo ./liveornot && sudo apt update
```

---

## 📄 许可证

MIT — 随便用，随便改，**出问题不负责** 😉

---

## 💬 有问题？

欢迎提 [Issue](https://github.com/YOUR_USER/liveornot/issues) 或 PR，一起让它更聪明！
