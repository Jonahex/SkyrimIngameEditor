using System.Globalization;
using System.Numerics;
using System.Reflection;
using DynamicData;
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
using Noggog.StructuredStrings.CSharp;

//var json = "[{\"Form\":{\"AngularVelocity\":{\"X\":0.0,\"Y\":0.0,\"Z\":0.0},\"DamagePerSecond\":0,\"DeepColor\":\"0, 21, 104, 102\",\"DepthNormals\":0.6499999761581421,\"DepthReflections\":1.0,\"DepthRefraction\":0.8999999761581421,\"DepthSpecularLighting\":0.5699999928474426,\"DisplacementDampner\":0.9800000190734863,\"DisplacementFalloff\":0.800000011920929,\"DisplacementFoce\":0.10000000149011612,\"DisplacementStartingSize\":0.699999988079071,\"DisplacementVelocity\":0.8500000238418579,\"Flags\":0,\"FlowNormalsNoiseTexture\":\"\",\"FogAboveWaterAmount\":0.949999988079071,\"FogAboveWaterDistanceFarPlane\":290.0,\"FogAboveWaterDistanceNearPlane\":0.0,\"FogUnderWaterAmount\":0.6000000238418579,\"FogUnderWaterDistanceFarPlane\":1880.0,\"FogUnderWaterDistanceNearPlane\":-1300.0,\"ImageSpace\":\"000946:Update.esm\",\"LinearVelocity\":{\"X\":0.1679999977350235,\"Y\":0.3474999964237213,\"Z\":0.0},\"Material\":\"null\",\"NoiseFalloff\":0.0,\"NoiseLayerOneAmplitudeScale\":0.44929999113082886,\"NoiseLayerOneTexture\":\"Data\\\\Textures\\\\Water\\\\DefaultWater.dds\",\"NoiseLayerOneUvScale\":1587.0,\"NoiseLayerOneWindDirection\":107.0,\"NoiseLayerOneWindSpeed\":0.02199999988079071,\"NoiseLayerThreeAmplitudeScale\":0.3659000098705292,\"NoiseLayerThreeTexture\":\"Data\\\\Textures\\\\Water\\\\DefaultWater.dds\",\"NoiseLayerThreeUvScale\":533.0,\"NoiseLayerThreeWindDirection\":198.0,\"NoiseLayerThreeWindSpeed\":0.06499999761581421,\"NoiseLayerTwoAmplitudeScale\":0.49639999866485596,\"NoiseLayerTwoTexture\":\"Data\\\\Textures\\\\Water\\\\DefaultWater.dds\",\"NoiseLayerTwoUvScale\":10000.0,\"NoiseLayerTwoWindDirection\":224.0,\"NoiseLayerTwoWindSpeed\":0.012000000104308128,\"Opacity\":100,\"OpenSound\":\"10EE72:Skyrim.esm\",\"ReflectionColor\":\"0, 38, 63, 74\",\"ShallowColor\":\"0, 194, 0, 255\",\"SpecularBrightness\":53.5,\"SpecularPower\":7500.0,\"SpecularRadius\":8949.0,\"SpecularSunPower\":1162.0,\"SpecularSunSparkleMagnitude\":1.0,\"SpecularSunSparklePower\":1514.0,\"SpecularSunSpecularMagnitude\":1.0,\"Spell\":\"null\",\"WaterFresnel\":0.3499999940395355,\"WaterReflectionMagnitude\":0.41999998688697815,\"WaterReflectivity\":0.550000011920929,\"WaterRefractionMagnitude\":39.0},\"FormKey\":\"048C2B:Skyrim.esm\",\"Master\":\"Skyrim.esm\",\"Override\":\"Update.esm\",\"References\":[\"Update.esm\",\"Skyrim.esm\"]},{\"Form\":{\"Flags\":187,\"ImageSpace\":\"0D485E:Skyrim.esm\",\"Lighting\":{\"AmbientColor\":\"0, 85, 75, 62\",\"AmbientColors\":{\"DirectionalXMinus\":\"0, 76, 75, 59\",\"DirectionalXPlus\":\"0, 93, 75, 64\",\"DirectionalYMinus\":\"0, 85, 71, 62\",\"DirectionalYPlus\":\"0, 85, 78, 62\",\"DirectionalZMinus\":\"0, 129, 114, 95\",\"DirectionalZPlus\":\"0, 71, 63, 50\",\"Scale\":1.0,\"Specular\":\"0, 0, 0, 0\"},\"DirectionalColor\":\"0, 83, 92, 102\",\"DirectionalFade\":0.0,\"DirectionalRotationXY\":0,\"DirectionalRotationZ\":0,\"FogClipDistance\":0.0,\"FogFar\":2700.0,\"FogFarColor\":\"0, 0, 0, 0\",\"FogMax\":1.0,\"FogNear\":0.0,\"FogNearColor\":\"0, 0, 0, 0\",\"FogPower\":1.0,\"Inherits\":132,\"LightFadeEndDistance\":4000.0,\"LightFadeStartDistance\":3500.0},\"LightingTemplate\":\"06175D:Skyrim.esm\",\"SkyAndWeatherFromRegion\":\"070408:Skyrim.esm\",\"Water\":\"074EEA:Skyrim.esm\",\"WaterEnvironmentMap\":\"Data\\\\Textures\\\\Cubemaps\\\\WRTemple_e.dds\"},\"FormKey\":\"0165A7:Skyrim.esm\",\"Master\":\"Skyrim.esm\",\"Override\":\"SC_KanarielleFollower.esp\",\"References\":[\"Skyrim.esm\",\"Update.esm\"]},{\"Form\":{\"AngularVelocity\":{\"X\":0.0,\"Y\":0.0,\"Z\":0.0},\"DamagePerSecond\":0,\"DeepColor\":\"0, 36, 47, 38\",\"DepthNormals\":0.550000011920929,\"DepthReflections\":0.20000000298023224,\"DepthRefraction\":0.8899999856948853,\"DepthSpecularLighting\":1.0,\"DisplacementDampner\":0.9800000190734863,\"DisplacementFalloff\":0.0,\"DisplacementFoce\":0.0,\"DisplacementStartingSize\":0.0,\"DisplacementVelocity\":0.0,\"Flags\":0,\"FlowNormalsNoiseTexture\":\"\",\"FogAboveWaterAmount\":0.0,\"FogAboveWaterDistanceFarPlane\":120.0,\"FogAboveWaterDistanceNearPlane\":0.0,\"FogUnderWaterAmount\":0.44999998807907104,\"FogUnderWaterDistanceFarPlane\":1000.0,\"FogUnderWaterDistanceNearPlane\":232.0,\"ImageSpace\":\"000946:Update.esm\",\"LinearVelocity\":{\"X\":0.0,\"Y\":0.0,\"Z\":0.0},\"Material\":\"0D6C11:Skyrim.esm\",\"NoiseFalloff\":4.0,\"NoiseLayerOneAmplitudeScale\":0.22460000216960907,\"NoiseLayerOneTexture\":\"Data\\\\Textures\\\\Water\\\\DefaultWater.dds\",\"NoiseLayerOneUvScale\":181.0,\"NoiseLayerOneWindDirection\":202.0,\"NoiseLayerOneWindSpeed\":0.0430000014603138,\"NoiseLayerThreeAmplitudeScale\":0.0,\"NoiseLayerThreeTexture\":\"Data\\\\Textures\\\\Water\\\\DefaultWater.dds\",\"NoiseLayerThreeUvScale\":145.0,\"NoiseLayerThreeWindDirection\":155.0,\"NoiseLayerThreeWindSpeed\":0.032999999821186066,\"NoiseLayerTwoAmplitudeScale\":0.22830000519752502,\"NoiseLayerTwoTexture\":\"Data\\\\Textures\\\\Water\\\\DefaultWater.dds\",\"NoiseLayerTwoUvScale\":254.0,\"NoiseLayerTwoWindDirection\":69.0,\"NoiseLayerTwoWindSpeed\":0.08699999749660492,\"Opacity\":0,\"OpenSound\":\"null\",\"ReflectionColor\":\"0, 44, 50, 47\",\"ShallowColor\":\"0, 255, 0, 247\",\"SpecularBrightness\":2.5,\"SpecularPower\":4203.0,\"SpecularRadius\":3116.0,\"SpecularSunPower\":3697.0,\"SpecularSunSparkleMagnitude\":3.0,\"SpecularSunSparklePower\":5106.0,\"SpecularSunSpecularMagnitude\":3.0,\"Spell\":\"null\",\"WaterFresnel\":0.05000000074505806,\"WaterReflectionMagnitude\":0.7599999904632568,\"WaterReflectivity\":0.5799999833106995,\"WaterRefractionMagnitude\":27.0},\"FormKey\":\"0CB11F:Skyrim.esm\",\"Master\":\"Skyrim.esm\",\"Override\":\"Update.esm\",\"References\":[\"Update.esm\",\"Skyrim.esm\"]},{\"Form\":null,\"FormKey\":\"074EE9:Skyrim.esm\",\"Master\":\"Skyrim.esm\",\"Override\":\"Skyrim.esm\",\"References\":[]}]";
//EspGenerator.EspGenerator.Export("Test.esp", json);

namespace EspGenerator
{
    public static class EspGenerator
    {
        private class InteriorCellLocator
        {
            public InteriorCellLocator(ICellBlockGetter block, ICellSubBlockGetter subBlock)
            {
                Block = block;
                SubBlock = subBlock;
            }
            public ICellBlockGetter Block { get; }
            public ICellSubBlockGetter SubBlock { get; }
        }
        private class ExteriorCellLocator
        {
            public ExteriorCellLocator(IWorldspaceGetter worldSpace)
            {
                Worldspace = worldSpace;
            }
            public ExteriorCellLocator(IWorldspaceGetter worldSpace, IWorldspaceBlockGetter? block, IWorldspaceSubBlockGetter? subBlock)
            {
                Worldspace = worldSpace;
                Block = block;
                SubBlock = subBlock;
            }
            public IWorldspaceGetter Worldspace { get;}
            public IWorldspaceBlockGetter? Block { get; }
            public IWorldspaceSubBlockGetter? SubBlock { get; }
        }
        private abstract class CellContainer
        {
            public CellContainer(ICellGetter originalCell)
            {
                var duplicateMask = new Cell.TranslationMask(true);
                duplicateMask.Persistent = false;
                duplicateMask.Temporary = false;
                duplicateMask.NavigationMeshes = false;
                duplicateMask.Landscape = false;
                Cell = originalCell.Duplicate(originalCell.FormKey, duplicateMask);
            }
            public Cell Cell { get; }

            public abstract void AddToMod(SkyrimMod esp);
        }

        private class InteriorCellPrototype : CellContainer
        {
            public InteriorCellPrototype(ICellGetter originalCell, InteriorCellLocator interiorLocator)
                : base(originalCell)
            {
                CellLocator = interiorLocator;
            }
            public InteriorCellLocator CellLocator { get; }
            public override void AddToMod(SkyrimMod esp)
            {
                var cellBlock = esp.Cells.Records.FirstOrDefault(x => x.BlockNumber == CellLocator.Block.BlockNumber);
                if (cellBlock is null)
                {
                    cellBlock = new CellBlock
                    {
                        BlockNumber = CellLocator.Block.BlockNumber,
                        GroupType = CellLocator.Block.GroupType,
                    };
                    esp.Cells.Records.Add(cellBlock);
                }
                var cellSubBlock = cellBlock.SubBlocks.FirstOrDefault(x => x.BlockNumber == CellLocator.SubBlock.BlockNumber);
                if (cellSubBlock is null)
                {
                    cellSubBlock = new CellSubBlock
                    {
                        BlockNumber = CellLocator.SubBlock.BlockNumber,
                        GroupType = CellLocator.SubBlock.GroupType,
                    };
                    cellBlock.SubBlocks.Add(cellSubBlock);
                }
                cellSubBlock.Cells.Add(Cell);
            }
        }

        private class ExteriorCellPrototype : CellContainer
        {
            public ExteriorCellPrototype(ICellGetter originalCell, ExteriorCellLocator exteriorLocator)
                : base(originalCell)
            {
                CellLocator = exteriorLocator;
            }
            public ExteriorCellLocator CellLocator { get; }
            public override void AddToMod(SkyrimMod esp)
            {
                var worldSpace = esp.Worldspaces.FirstOrDefault(x => x.FormKey == CellLocator.Worldspace.FormKey);
                if (worldSpace is null)
                {
                    var duplicateMask = new Worldspace.TranslationMask(true);
                    duplicateMask.SubCells = false;
                    duplicateMask.TopCell = false;
                    duplicateMask.LargeReferences = false;
                    worldSpace = CellLocator.Worldspace.Duplicate(CellLocator.Worldspace.FormKey, duplicateMask);
                    esp.Worldspaces.Add(worldSpace);
                }
                if (CellLocator.Block is null)
                {
                    worldSpace.TopCell = Cell;
                    return;
                }
                var cellBlock = worldSpace.SubCells.FirstOrDefault(x => x.BlockNumberX == CellLocator.Block.BlockNumberX && x.BlockNumberY == CellLocator.Block.BlockNumberY);
                if (cellBlock is null)
                {
                    cellBlock = new WorldspaceBlock
                    {
                        BlockNumberX = CellLocator.Block.BlockNumberX,
                        BlockNumberY = CellLocator.Block.BlockNumberY,
                        GroupType = CellLocator.Block.GroupType,
                    };
                    worldSpace.SubCells.Add(cellBlock);
                }
                var cellSubBlock = cellBlock.Items.FirstOrDefault(x => x.BlockNumberX == CellLocator.SubBlock.BlockNumberX && x.BlockNumberY == CellLocator.SubBlock.BlockNumberY);
                if (cellSubBlock is null)
                {
                    cellSubBlock = new WorldspaceSubBlock
                    {
                        BlockNumberX = CellLocator.SubBlock.BlockNumberX,
                        BlockNumberY = CellLocator.SubBlock.BlockNumberY,
                        GroupType = CellLocator.SubBlock.GroupType,
                    };
                    cellBlock.Items.Add(cellSubBlock);
                }
                cellSubBlock.Items.Add(Cell);
            }
        }

        private static InteriorCellLocator? LocateInteriorCell(ISkyrimModGetter mod, ICellGetter cell)
        {
            foreach (var block in mod.Cells.Records)
            {
                foreach (var subBlock in block.SubBlocks)
                {
                    if (subBlock.Cells.Any(x => x.FormKey == cell.FormKey))
                    {
                        return new InteriorCellLocator(block, subBlock);
                    }
                }
            }
            return null;
        }

        private static ExteriorCellLocator? LocateExteriorCell(ISkyrimModGetter mod, ICellGetter cell)
        {
            foreach (var worldSpace in mod.Worldspaces.Records)
            {
                if (worldSpace.TopCell != null && worldSpace.TopCell.FormKey == cell.FormKey)
                {
                    return new ExteriorCellLocator(worldSpace);
                }
                foreach (var block in worldSpace.SubCells)
                {
                    foreach (var subBlock in block.Items)
                    {
                        if (subBlock.Items.Any(x => x.FormKey == cell.FormKey))
                        {
                            return new ExteriorCellLocator(worldSpace, block, subBlock);
                        }
                    }
                }
            }
            return null;
        }

        private static CellContainer? MakeCellContainer(ISkyrimModGetter srcMod, ICellGetter srcCell)
        {
            if (srcCell.Flags.HasFlag(Cell.Flag.IsInteriorCell))
            {
                var cellLocator = LocateInteriorCell(srcMod, srcCell);
                if (cellLocator != null)
                {
                    return new InteriorCellPrototype(srcCell, cellLocator);
                }
            }
            else
            {
                var cellLocator = LocateExteriorCell(srcMod, srcCell);
                if (cellLocator != null)
                {
                    return new ExteriorCellPrototype(srcCell, cellLocator);
                }
            }
            return null;
        }

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
                var envBuilder = ctor.Invoke(new object[] { new GameReleaseInjection(GameRelease.SkyrimSE), new DataDirectoryInjection(new DirectoryPath(Environment.CurrentDirectory + "\\Data\\")), null, null, null }) as GameEnvironmentBuilder;
                //var envBuilder = ctor.Invoke(new object[] { new GameReleaseInjection(GameRelease.SkyrimSE), new DataDirectoryInjection(new DirectoryPath("D:\\Games\\Steam\\steamapps\\common\\Skyrim Special Edition\\Data\\")), null, null, null }) as GameEnvironmentBuilder;
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

                var cellData = new Dictionary<FormKey, CellContainer>();
                foreach (JObject token in json.ToArray())
                {
                    var formKey = token["FormKey"].ToObject<FormKey>(jsonSerializer);
                    var recordData = token["Form"].ToString();

                    var link = formKey.ToLink<IMajorRecordGetter>();
                    if (link.TryResolve(env.LinkCache, out var foundRecord))
                    {
                        var srcMod = env.LoadOrder[foundRecord.FormKey.ModKey].Mod as ISkyrimModGetter;
                        if (foundRecord is ICellGetter originalCell)
                        {
                            if (!cellData.TryGetValue(originalCell.FormKey, out var cellContainer))
                            {
                                cellContainer = MakeCellContainer(srcMod, originalCell);
                                if (cellContainer != null)
                                {
                                    cellData.Add(originalCell.FormKey, cellContainer);
                                }
                            }
                            if (cellContainer != null)
                            {
                                try
                                {
                                    JsonConvert.PopulateObject(recordData, cellContainer.Cell, serializerSettings);
                                }
                                catch (Exception e) { streamWriter.WriteLine(e.ToString()); }
                            }
                        }
                        else if (foundRecord is IPlacedGetter originalPlaced)
                        {
                            var cellFormKey = token["Form"]["Cell"].ToObject<FormKey>(jsonSerializer);
                            var cellLink = cellFormKey.ToLink<IMajorRecordGetter>();
                            if (cellLink.TryResolve(env.LinkCache, out ICellGetter foundCell))
                            {
                                if (!cellData.TryGetValue(foundCell.FormKey, out var cellContainer))
                                {
                                    cellContainer = MakeCellContainer(srcMod, foundCell);
                                    if (cellContainer != null)
                                    {
                                        cellData.Add(foundCell.FormKey, cellContainer);
                                    }
                                }
                                if (cellContainer != null)
                                {
                                    bool isPersistent = (originalPlaced.MajorRecordFlagsRaw & 0x400u) > 0;
                                    IPlaced placedDuplicate = originalPlaced.Duplicate(formKey) as IPlaced;
                                    try
                                    {
                                        JsonConvert.PopulateObject(recordData, placedDuplicate, serializerSettings);
                                    }
                                    catch (Exception e) { streamWriter.WriteLine(e.ToString()); }
                                    if (isPersistent)
                                    {
                                        cellContainer.Cell.Persistent.Add(placedDuplicate);
                                    }
                                    else
                                    {
                                        cellContainer.Cell.Temporary.Add(placedDuplicate);
                                    }
                                }
                            }
                        }
                        else
                        {
                            var record = foundRecord.Duplicate(formKey);
                            try
                            {
                                JsonConvert.PopulateObject(recordData, record, serializerSettings);
                            }
                            catch(Exception e) { streamWriter.WriteLine(e.ToString()); }
                            var group = esp.TryGetTopLevelGroup(record.GetType());
                            if (!(group is null))
                            {
                                group.AddUntyped(record);
                            }
                        }
                    }
                }

                foreach (var (formKey, cellContainer) in cellData)
                {
                    cellContainer.AddToMod(esp);
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
