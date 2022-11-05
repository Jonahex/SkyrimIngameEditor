using Mutagen.Bethesda.Plugins.Assets;
using Newtonsoft.Json;
using Noggog;

namespace EspGenerator
{
    public sealed class AssetLinkJsonConverter : JsonConverter
    {
        public override bool CanConvert(Type objectType)
        {
            return objectType.IsGenericType && (objectType.GetGenericTypeDefinition() == typeof(AssetLink<>) || objectType.GetGenericTypeDefinition() == typeof(IAssetLink<>));
        }

        public override object? ReadJson(JsonReader reader, Type objectType, object? existingValue, JsonSerializer serializer)
        {
            var obj = reader.Value;
            if (obj == null)
            {
                return null;
            }

            var str = obj.ToString();

            return Activator.CreateInstance(typeof(AssetLink<>).MakeGenericType(objectType.GenericTypeArguments[0]),
                str);
        }

        public override void WriteJson(JsonWriter writer, object? value, JsonSerializer serializer)
        {
            if (value == null) return;
            writer.WriteValue(((IAssetLink)value).RawPath);
        }
    }
}
