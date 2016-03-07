package Common;

import java.net.InetAddress;

import com.google.gson.annotations.Expose;

/**
 * this is a data class used to store and find the middlebox within the
 * repository
 */
public class Middlebox implements IChainNode {
	@Override
	public String toString() {
		return "Middlebox [id=" + id + ", name=" + name + "]";
	}

	public String id;
	public String name;
	@Expose
	public InetAddress address;

	public Middlebox(String id, String name) {
		this.id = id;
		this.name = name;
	}

	public Middlebox(String id) {
		this.id = id;
	}

	@Override
	public boolean equals(Object o) {
		if (this == o)
			return true;
		if (o == null || getClass() != o.getClass())
			return false;

		Middlebox that = (Middlebox) o;

		if (!id.equals(that.id))
			return false;

		return true;
	}

	@Override
	public int hashCode() {
		return id.hashCode();
	}

	@Override
	public InetAddress GetAddress() {
		return address;
	}
}
