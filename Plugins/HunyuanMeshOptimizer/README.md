# HunyuanMeshOptimizer

把 Hunyuan3D 生成的高面数 `.glb` 模型一键减面成 LOD0/LOD1/LOD2，并可选直接导入 Content Browser。

## 依赖

- 项目已配置的 Hunyuan3D Python 环境：`hunyuan/venv/Scripts/python.exe`
- 减面脚本：`hunyuan/decimate_glb.py`（使用 pymeshlab + trimesh）

## 使用方法

1. 启动 Unreal Editor 并加载本项目（插件已在 `TA-Playground.uproject` 中启用）。
2. 在编辑器主工具栏点击 **Hunyuan LOD** 按钮。
3. 在弹出的对话框中：
   - **Input GLB**：选择 `hybrid_pipeline.py` 生成的高面数 `.glb` 文件。
   - **Output Directory**：选择 LOD 文件输出目录（不存在会自动创建）。
   - **Target Face Counts**：按需修改 LOD0/LOD1/LOD2 目标面数（默认 20000 / 8000 / 2500）。
   - **Import to Content Browser**：勾选后，生成的 LOD 会自动导入到 **Destination Package Path**（默认 `/Game/Hunyuan_LODs`）。
4. 点击 **Generate LODs**，等待 Python 减面脚本执行完成。
5. 成功后会在右下角弹出通知；若失败则会显示 stdout/stderr。

## 命令行等价操作

不使用插件界面时，可以直接在虚拟环境中运行：

```powershell
cd hunyuan
.\venv\Scripts\activate
python decimate_glb.py <input.glb> --output-dir <dir> --lod0 20000 --lod1 8000 --lod2 2500
```

输出文件为 `<input_stem>_LOD0.glb`、`<input_stem>_LOD1.glb`、`<input_stem>_LOD2.glb`。
