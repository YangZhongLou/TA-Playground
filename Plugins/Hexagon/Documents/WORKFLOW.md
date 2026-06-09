# Hexagon Plugin Workflow

## Rule: Unit Tests Must Pass Before Verification

每次修改 C++ 代码后，**必须先跑通单元测试**，再让用户验证。不允许跳过测试直接让用户看。

### 执行命令

```powershell
# 1. 编译
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" `
  -Target="TAPlaygroundEditor Win64 Development" `
  -Project="D:\Mine\unreal_projects\TA-Playground\TA-Playground.uproject" `
  -WaitMutex -FromMsBuild

# 2. 跑测试 (headless)
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\Mine\unreal_projects\TA-Playground\TA-Playground.uproject" `
  -ExecCmds="Automation RunTests Hexagon; Quit" `
  -NullRHI -Unattended -NoSplash -Log

# 3. 检查结果
grep -c "Result={Fail}" "Saved/Logs/TA-Playground.log"
# 期望输出: 0
```

### 不合格条件（必须修好才能提交验证）

- 编译不通过
- 任何 Hexagon 测试失败 (Result={Fail})
- 编译警告（新增的）

### 测试列表

运行 `Automation RunTests Hexagon` 会执行 58 个测试：

| 分组 | 测试数 | 覆盖 |
|------|--------|------|
| `TA.Hexagon.FHexCoord.*` | 3 | 坐标运算符、哈希 |
| `TA.Hexagon.FHexGeometry.*` | 4 | 距离、邻居、螺旋、往返 |
| `TA.Hexagon.FHexPrismGen.*` | 5 | 基础生成、LOD、UV |
| `TA.Hexagon.FHexTerrainGen.*` | 5 | 分类、颜色、生成、边界 |
| `TA.Hexagon.DebugColor.*` | 1 | 调试色哈希 |
| `Hexagon.Geometry.*` | 28 | 全部几何数学 |
| `Hexagon.Terrain.*` | 12 | 分类、颜色、生成、噪声、LOD、Chunk |

### 新增测试

测试文件放在 `Source/Hexagon/Private/Tests/`，遵循命名规范 `{Category}Test.cpp`。

```cpp
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMyTest, "Hexagon.Category.TestName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMyTest::RunTest(const FString&)
{
    TestEqual("description", actual, expected);
    TestTrue("condition", condition);
    return true;
}
```
