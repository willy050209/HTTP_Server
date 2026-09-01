using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Http;
using System.Text.Json;
using System.Text.Json.Serialization;

var builder = WebApplication.CreateBuilder(args);
builder.Logging.ClearProviders();

builder.WebHost.UseKestrel(options =>
{
    options.AddServerHeader = false;
});

var app = builder.Build();

// 1. Plaintext Endpoint
app.MapGet("/plaintext", () => Results.Text("Hello, World!", "text/plain"));

// 2. JSON Endpoint
app.MapGet("/json", () => Results.Json(new JsonMessage("Hello, World!", 1700000000), JsonContext.Default.JsonMessage));

// 3. Dynamic Route Parameter Endpoint
app.MapGet("/users/{id:int}", (int id) => Results.Json(new UserResponse(id, $"User_{id}", "active"), JsonContext.Default.UserResponse));

app.Run();

public record JsonMessage(string message, long timestamp);
public record UserResponse(int id, string name, string status);

[JsonSerializable(typeof(JsonMessage))]
[JsonSerializable(typeof(UserResponse))]
internal partial class JsonContext : JsonSerializerContext
{
}
