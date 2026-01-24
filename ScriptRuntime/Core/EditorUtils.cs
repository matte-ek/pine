using System.IO;
using System.Xml;

namespace Pine.Core
{
    internal class EditorUtils
    {
        private const string ScriptFilePath = "game/runtime/ScriptFiles.props";
        
        internal static void AddScriptFile(string filePath)
        {
            Log.Verbose($"EditorUtils: Creating script file {filePath}");

            CopyTemplate(filePath);
            AddFileToProject(filePath);
        }
        
        internal static void RemoveScriptFile(string filePath)
        {
            Log.Verbose($"EditorUtils: Removing script file {filePath}");

            DeleteFileFromProject(filePath);
        }

        private static void CopyTemplate(string filePath)
        {
            var fileName = Path.GetFileNameWithoutExtension(filePath);
            var templateData = File.ReadAllText("game/runtime/ScriptTemplate.cs");
            
            templateData = templateData.Replace("%FileName%", fileName);

            File.WriteAllText(filePath, templateData);
        }

        private static void DeleteFileFromProject(string filePath)
        {
            var document = new XmlDocument();

            document.Load(ScriptFilePath);

            var includeToRemove = $"../assets/{filePath.Substring(12)}";
            
            var node = document.SelectSingleNode($"/Project/ItemGroup/Compile[@Include='{includeToRemove}']");
            if (node == null)
            {
                return;
            }

            node.ParentNode?.RemoveChild(node);
            
            document.Save(ScriptFilePath);
        }

        private static void AddFileToProject(string filePath)
        {
            var document = new XmlDocument();
            
            document.Load(ScriptFilePath);

            var itemGroup = document.SelectSingleNode("/Project/ItemGroup");
            if (itemGroup == null)
            {
                return;
            }

            var element = document.CreateElement("Compile");
            
            element.SetAttribute("Include", $"../assets/{filePath.Substring(12)}");
            
            var linkElement = document.CreateElement("Link");
            
            linkElement.InnerText = filePath.Substring(12);
            
            element.AppendChild(linkElement);
            itemGroup.AppendChild(element);
            
            document.Save(ScriptFilePath);
        }
    }
}