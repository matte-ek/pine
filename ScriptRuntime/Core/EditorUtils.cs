namespace Pine.Core
{
    // Script source files are now managed engine-side: the editor writes a '.cs' next to its
    // '.passet' in the project's assets/ tree, and the project's Game.csproj compiles them via a
    // glob (assets/**/*.cs). The previous ScriptFiles.props / template management that lived here
    // is therefore no longer needed.
    internal class EditorUtils
    {
    }
}
