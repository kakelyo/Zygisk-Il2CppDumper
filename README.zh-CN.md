# Zygisk-Il2CppDumper
Zygisk版Il2CppDumper，在游戏运行时dump il2cpp数据，可以绕过保护，加密以及混淆。

## 如何食用
1. 在root的安卓手机上，安装[Magisk](https://github.com/topjohnwu/Magisk) v24以上版本并开启Zygisk
2. 生成模块
   - GitHub Actions
      1. Fork这个项目
      2. 在你fork的项目中选择**Actions**选项卡
      3. 在左边的侧边栏中，单击**Build**
      4. 选择**Run workflow**
      5. 输入游戏包名并点击**Run workflow**
      6. 等待操作完成并下载zygisk-il2cppdumper.zip
3. 把zygisk-il2cppdumper.zip复制root的到安卓手机上
4. 在Magisk里点 模块 菜单，再点从本地安装，选择 zygisk-il2cppdumper.zip
5. 在手机上安装要反编译的游戏，然后启动游戏，会在`/data/data/游戏包名/files/`目录下生成`dump.cs`
