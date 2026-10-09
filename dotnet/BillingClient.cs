using System.Net.Sockets;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;

namespace Hyperion.Billing;

public sealed class BillingClient : IDisposable
{
    readonly HttpClient http;
    public BillingClient(string socket, string keyFile)
    {
        var handler = new SocketsHttpHandler { AllowAutoRedirect = false, ConnectCallback = async (_,ct) => {
            var s = new Socket(AddressFamily.Unix, SocketType.Stream, ProtocolType.Unspecified);
            try { await s.ConnectAsync(new UnixDomainSocketEndPoint(socket),ct); return new NetworkStream(s,true); } catch { s.Dispose(); throw; }
        }};
        http = new HttpClient(handler) { BaseAddress = new Uri("http://localhost/v1/"), Timeout = TimeSpan.FromSeconds(3) };
        http.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer",File.ReadAllText(keyFile).Trim());
    }
    public async Task<JsonElement> Get(string path, CancellationToken ct = default)
    {
        using var response = await http.GetAsync(path,ct); response.EnsureSuccessStatusCode(); return await response.Content.ReadFromJsonAsync<JsonElement>(ct);
    }
    public async Task<JsonElement?> Post(string path, object value, CancellationToken ct = default)
    {
        using var response = await http.PostAsJsonAsync(path,value,ct);
        if(!response.IsSuccessStatusCode) { var problem = await response.Content.ReadAsStringAsync(ct); throw new InvalidOperationException(problem); }
        return response.StatusCode == System.Net.HttpStatusCode.NoContent ? null : await response.Content.ReadFromJsonAsync<JsonElement>(ct);
    }
    public async Task<bool> MainAllowed(CancellationToken ct = default)
    {
        try { return (await Get("status",ct)).GetProperty("mainAllowed").GetBoolean(); } catch(Exception e) when(e is HttpRequestException or TaskCanceledException or IOException) { return false; }
    }
    public void Dispose() => http.Dispose();
}
