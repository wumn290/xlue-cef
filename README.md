# xlue-cef
迅雷BOLT引擎中使用cef浏览器控件

原理很简单，就是把cefclient的浏览器窗口句柄设置在xlue的RealObject上去
提交上来记录一下， 方便以后使用， 顺便练习一下用git命令行工具提交代码到github

使用vs2008编译， 编译前，将stdint.h拷贝到C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\include

注释：
1、cef是从http://opensource.spotify.com/cefbuilds/index.html下载的win32 release版
版本号是3.2840.1511
2、xlue代码从在demo里面HelloBolt6的基础上改的
