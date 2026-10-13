# Working Rules

>[!IMPORTANT]
> You must strictly follow the rules defined in this file for all subsequent actions.

- Do not modify, create, delete, move, rename, install, start services, or run state-changing commands unless I explicitly approve that exact action first.
- Before making any change, explain what you intend to do and wait for my clear confirmation.
- Read-only inspection is allowed unless I say otherwise.
- If a command may change files, project state, settings, dependencies, network state, or external accounts, ask first.
- Never use destructive commands such as `git reset --hard`, recursive delete, or force overwrite unless I explicitly request that exact command.
- 所有自訂錯誤訊息（包含 throw、ThrowIfFailed 與錯誤日誌）統一使用「ClassName::FunctionName : 錯誤原因」；自由函式使用「Namespace::FunctionName : 錯誤原因」，無 namespace 時使用「FunctionName : 錯誤原因」。前綴必須對應實際發生錯誤的函式，第三方錯誤內容須附在此前綴之後。

## Additional references
- For D3D12, rendering, shader, or graphics pipeline work, read `D3D12Rule.md` before making recommendations or changes.