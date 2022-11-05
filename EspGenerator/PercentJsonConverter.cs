using Newtonsoft.Json;
using Noggog;

namespace EspGenerator
{
    public sealed class PercentJsonConverter : JsonConverter
    {
        public override bool CanConvert(Type objectType)
        {
            return objectType == typeof(Percent);
        }

        public override object? ReadJson(JsonReader reader, Type objectType, object? existingValue, JsonSerializer serializer)
        {
            var obj = reader.Value;
            if (obj == null)
            {
                return null;
            }

            var str = obj.ToString();
            Double.TryParse(str, out var value);
            return new Percent(value);
        }

        public override void WriteJson(JsonWriter writer, object? value, JsonSerializer serializer)
        {
            if (value == null) return;
            writer.WriteValue(((Percent)value).Value);
        }
    }
}
