using System.Reflection;
using Mutagen.Bethesda;
using Mutagen.Bethesda.Environments;
using Mutagen.Bethesda.Environments.DI;
using Mutagen.Bethesda.Json;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Records;
using Mutagen.Bethesda.Skyrim;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Noggog;

namespace EspGenerator
{
    public static class EspGenerator
    {
        public static int Export(string outputPath, string jsonString)
        {
            //var currentDirectory = Environment.CurrentDirectory;
            var currentDirectory = Environment.CurrentDirectory + "\\Data\\SKSE\\plugins\\EspGenerator\\";
            AppDomain.CurrentDomain.SetData("APP_CONTEXT_BASE_DIRECTORY", currentDirectory);

            var streamWriter = new StreamWriter(currentDirectory + "EspGenerator.log");
            streamWriter.WriteLine($"Writing {jsonString} to {outputPath}");
            
            try
            {
                var serializerSettings = new JsonSerializerSettings
                {
                    Formatting = Formatting.Indented,
                    ReferenceLoopHandling = ReferenceLoopHandling.Ignore,
                };
                serializerSettings.AddMutagenConverters();
                serializerSettings.Converters.Add(new PercentJsonConverter());
                serializerSettings.Converters.Add(new AssetLinkJsonConverter());
                serializerSettings.Converters.Add(new ExtendedListConverter());

                var jsonSerializer = JsonSerializer.Create(serializerSettings);

                var esp = new SkyrimMod(ModKey.FromNameAndExtension(Path.GetFileName(outputPath)), SkyrimRelease.SkyrimSE);

                ConstructorInfo ctor = typeof(GameEnvironmentBuilder).GetTypeInfo().DeclaredConstructors.ToArray()[1];
                var envBuilder = ctor.Invoke(new object[] { new GameReleaseInjection(GameRelease.SkyrimSE), new DataDirectoryInjection(new DirectoryPath(Environment.CurrentDirectory + "\\Data\\")), null,
                    null, null }) as GameEnvironmentBuilder;
                //var envBuilder = GameEnvironmentBuilder.Create(GameRelease.SkyrimSE);

                JToken[] json = JToken.Parse(jsonString).ToArray();
                
                HashSet<ModKey> modKeys = new HashSet<ModKey>();
                foreach (JObject token in json.ToArray())
                {
                    modKeys.Add(ModKey.FromFileName(token["Master"].ToString()));
                    modKeys.Add(ModKey.FromFileName(token["Override"].ToString()));
                    foreach (var reference in token["References"].ToArray())
                    {
                        modKeys.Add(ModKey.FromFileName(reference.ToString()));
                    }
                }
                
                var env = envBuilder.WithLoadOrder(modKeys.ToArray()).WithOutputMod(esp).Build();

                foreach (JObject token in json.ToArray())
                {
                    var formKey = token["FormKey"].ToObject<FormKey>(jsonSerializer);
                    var recordData = token["Form"].ToString();

                    var link = formKey.ToLink<IMajorRecordGetter>();
                    if (link.TryResolve(env.LinkCache, out var foundRecord))
                    {
                        //streamWriter.WriteLine(JsonConvert.SerializeObject(foundRecord, serializerSettings));
                        var record = foundRecord.Duplicate(formKey);
                        JsonConvert.PopulateObject(recordData, record, serializerSettings);
                        esp.GetTopLevelGroup(record.GetType()).AddUntyped(record);
                    }
                }

                esp.WriteToBinary(outputPath);

                streamWriter.Close();
                return 0;
            }
            catch (Exception e)
            {
                streamWriter.WriteLine(e.ToString());
                streamWriter.Close();
                return 1;
            }
        }
    }
}
